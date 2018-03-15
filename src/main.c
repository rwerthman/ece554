/**
 * TODO:
 * 1. Buffering for ADC input so all of the input will be drawn?
 * 2. Semaphores or mailboxes for shared resources between tasks
 * 3. Pull out scheduler into separate header and c file
 *
 * High pitch = small period
 * Low pitch = high period
 */

#include <msp430.h>
#include <driverlib.h>
#include <grlib.h>
#include "inc/Crystalfontz128x128_ST7735.h"
#include "inc/semaphore.h"
#include "inc/scheduler.h"

#define MCLK_SMCLK_DESIRED_FREQUENCY_IN_KHZ 25000
#define NUM_ALIENS (uint8_t)3
#define NUM_EXPLOSIONS (uint8_t)3
#define NUM_BULLETS (uint8_t)10
#define NUM_TASKS (uint8_t)4
#define STACK_SIZE 64 // 64*32 = 2048 btyes

/* Init function */
void initWatchdog(void);
void initClocks(void);
void initTimers(void);
void initGPIO(void);
void initJoystickADC(void);
void initObjects(void);

/* Utilities */
uint32_t mapValToRange(uint32_t x, uint32_t input_min, uint32_t input_max,
                       uint32_t output_min, uint32_t output_max);

/* Task globals */
/* Tasks to be run */
void drawSpacecraft(void);
uint32_t drawSpacecraftStack[STACK_SIZE];

void drawAliens(void);
uint32_t drawAliensStack[STACK_SIZE];

void drawExplosions(void);
uint32_t drawExplosionsStack[STACK_SIZE];

void detectAndDebounceFireButtonPress(void);
uint32_t detectAndDebounceFireButtonPressStack[STACK_SIZE];

uint32_t *stack_pointer[NUM_TASKS]; // Stores the stack pointer for each task
int8_t currentRunningTask;
uint32_t *tempSP;

/* Semaphores for the graphics context */
semCaller semC1 =
{
 0,
 0
};
semCaller semC2 =
{
 1,
 0
};
semCaller semC3 =
{
 2,
 0
};
semType sem1 =
{
 1,
 0
};

/* Sound globals */
uint8_t pwmFireCounter = 4;
uint8_t pwmExplosionCounter = 4;

Graphics_Context g_sContext;

/* Spacecraft globals */
static uint32_t spacecraftPosition[2];
static uint32_t previousSpacecraftPosition[2];
Graphics_Rectangle spacecraftRect;

/* Bullet globals */
Graphics_Rectangle bulletsRect;
uint16_t bullets[NUM_BULLETS ][2];
uint16_t previousBullets[NUM_BULLETS ][2];
uint8_t currentBullet = 0;
/* Fire button S2  value */
uint8_t currentFireButtonState;
uint8_t previousFireButtonState;

/* Explosion globals */
enum
{
  x = 0, y = 1, s = 2, // Enable or not
  r = 3, // Radius
  d = 4  // Direction
};
uint16_t explosions[NUM_ALIENS ][5];
Graphics_Rectangle explosionRect;

/* Aliens globals */
Graphics_Rectangle alienRect;
uint16_t aliens[NUM_ALIENS ][2];
uint16_t previousAliens[NUM_ALIENS ][2];

/**
 * main.c
 */
void main(void)
{

  initWatchdog();
  initClocks();
  initTimers();
  initGPIO();
  initJoystickADC();
  initObjects();

  /* Initializes display */
  Crystalfontz128x128_Init();

  /* Set default screen orientation */
  Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP);

  /* Initializes graphics context */
  Graphics_initContext(&g_sContext, &g_sCrystalfontz128x128);
  Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_GREEN);
  Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_RED);
  GrContextFontSet(&g_sContext, &g_sFontFixed6x8);
  Graphics_clearDisplay(&g_sContext);

  stack_pointer[0] = initializeStack(drawSpacecraft,
                                     &drawSpacecraftStack[STACK_SIZE - 1]);
  stack_pointer[1] = initializeStack(drawAliens,
                                       &drawAliensStack[STACK_SIZE - 1]);
  stack_pointer[2] = initializeStack(drawExplosions,
                                       &drawExplosionsStack[STACK_SIZE - 1]);
  stack_pointer[3] = initializeStack(detectAndDebounceFireButtonPress,
                                         &detectAndDebounceFireButtonPressStack[STACK_SIZE - 1]);

  currentRunningTask = NUM_TASKS;

  // Globally enable interrupts
  __bis_SR_register(GIE);

  // Enable/Start first sampling and conversion cycle
  ADC12_A_startConversion(ADC12_A_BASE,
  ADC12_A_MEMORY_0,
                          ADC12_A_REPEATED_SEQOFCHANNELS);

  // Start the timer to trigger the Joystick ADC and scheduler
  Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);
  // Start the timer to trigger the Aliens
  Timer_A_startCounter(TIMER_A1_BASE, TIMER_A_UP_MODE);

  while (1)
  {

  }
}

void initWatchdog(void)
{
  // Stop the watchdog timer
  WDT_A_hold( WDT_A_BASE);
}

void initObjects(void)
{
  uint8_t i;
  for (i = 0; i < NUM_BULLETS ; i++)
  {
    bullets[i][x] = 200;
    bullets[i][y] = 200;
    previousBullets[i][x] = bullets[i][x];
    previousBullets[i][y] = bullets[i][y];
  }

  for (i = 0; i < NUM_ALIENS ; i++)
  {
    aliens[i][x] = 2 + 10 * i;
    aliens[i][y] = 2 + 10 * i;
    previousAliens[i][x] = aliens[i][x];
    previousAliens[i][y] = aliens[i][y];

    /* Disable the explosions */
    explosions[i][s] = 0;

  }

  spacecraftPosition[x] = 0;
  spacecraftPosition[y] = 0;

  previousSpacecraftPosition[x] = spacecraftPosition[x];
  previousSpacecraftPosition[y] = spacecraftPosition[y];
}

void initGPIO(void)
{
  /* Configures Pin 6.3 (Joystick Y) and 6.5 (Joystick X) as ADC input */
  GPIO_setAsInputPin(GPIO_PORT_P6, GPIO_PIN3);
  GPIO_setAsInputPin(GPIO_PORT_P6, GPIO_PIN5);

  /* Enable user button 2 on educational booster pack */
  GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P3, GPIO_PIN7);

  /* Enable buzzer on educational booster pack with timer output going to this pin*/
  GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P2, GPIO_PIN5);

}

void initClocks(void)
{
  // Set the core voltage level to handle a 25 MHz clock rate
  PMM_setVCore(PMM_CORE_LEVEL_3);

  // Set ACLK to use REFO as its oscillator source (32KHz)
  UCS_initClockSignal(UCS_ACLK,
  UCS_REFOCLK_SELECT,
                      UCS_CLOCK_DIVIDER_1);

  // Set REFO as the oscillator reference clock for the FLL
  UCS_initClockSignal(UCS_FLLREF,
  UCS_REFOCLK_SELECT,
                      UCS_CLOCK_DIVIDER_1);

  // Set MCLK and SMCLK to use the DCO/FLL as their oscillator source (25MHz)
  // The function does a number of things: Calculates required FLL settings; Configures FLL and DCO,
  // and then sets MCLK and SMCLK to use the DCO (with FLL runtime calibration)
  UCS_initFLLSettle(
      MCLK_SMCLK_DESIRED_FREQUENCY_IN_KHZ,
      (MCLK_SMCLK_DESIRED_FREQUENCY_IN_KHZ / (UCS_REFOCLK_FREQUENCY / 1024)));
}

/**
 * @brief Enable the joystick analog to digital converter
 */
void initJoystickADC(void)
{

  /* TA0.1 is source ADC12_A_SAMPLEHOLDSOURCE_1 of this ADC */
  ADC12_A_init(ADC12_A_BASE,
  ADC12_A_SAMPLEHOLDSOURCE_1,
               ADC12_A_CLOCKSOURCE_SMCLK,
               ADC12_A_CLOCKDIVIDER_1);

  ADC12_A_enable(ADC12_A_BASE);

  // Change the resolution to be between 0 - 127 to work with the
  // pixels on the LCD
  ADC12_A_setResolution(ADC12_A_BASE, ADC12_A_RESOLUTION_8BIT);

  ADC12_A_setupSamplingTimer(ADC12_A_BASE,
  ADC12_A_CYCLEHOLD_16_CYCLES,
                             ADC12_A_CYCLEHOLD_16_CYCLES,
                             ADC12_A_MULTIPLESAMPLESDISABLE);

  /* Joystick X memory input */
  ADC12_A_configureMemoryParam param0 = {
  ADC12_A_MEMORY_0,
                                          ADC12_A_INPUT_A5,
                                          ADC12_A_VREFPOS_AVCC,
                                          ADC12_A_VREFNEG_AVSS,
                                          ADC12_A_NOTENDOFSEQUENCE };

  ADC12_A_configureMemory(ADC12_A_BASE, &param0);

  /* Joystick Y memory input */
  ADC12_A_configureMemoryParam param1 = {
  ADC12_A_MEMORY_1,
                                          ADC12_A_INPUT_A3,
                                          ADC12_A_VREFPOS_AVCC,
                                          ADC12_A_VREFNEG_AVSS,
                                          ADC12_A_ENDOFSEQUENCE };

  ADC12_A_configureMemory(ADC12_A_BASE, &param1);

  // Enable memory buffer 1 interrupts
  // so interrupt fires when sequence mem0 - mem1 or x - y is completed
  ADC12_A_clearInterrupt(ADC12_A_BASE,
  ADC12IFG1);
  ADC12_A_clearInterrupt(ADC12_A_BASE,
  ADC12IFG0);
  //ADC12_A_enableInterrupt(ADC12_A_BASE, ADC12IE1);
}

// https://stackoverflow.com/questions/5731863/mapping-a-numeric-range-onto-another
// https://github.com/arduino/Arduino/issues/2466
uint32_t mapValToRange(uint32_t x, uint32_t input_min, uint32_t input_max,
                       uint32_t output_min, uint32_t output_max)
{
  uint32_t val = (uint32_t) (((x - input_min) * (output_max - output_min + 1)
      / (input_max - input_min + 1)) + output_min);
  return val;
}

void initTimers(void)
{

  /* Joystick ADC timer */
  Timer_A_initUpModeParam upModeParam = {
  TIMER_A_CLOCKSOURCE_ACLK, // 32 KHz clock => Period is .031 milliseconds
      TIMER_A_CLOCKSOURCE_DIVIDER_1, 320,
      TIMER_A_TAIE_INTERRUPT_DISABLE,
      TIMER_A_CCIE_CCR0_INTERRUPT_DISABLE,
      TIMER_A_DO_CLEAR,
      false };
  Timer_A_initUpMode(TIMER_A0_BASE, &upModeParam);

  Timer_A_initCompareModeParam compareParam1 = {
      TIMER_A_CAPTURECOMPARE_REGISTER_1,
      TIMER_A_CAPTURECOMPARE_INTERRUPT_DISABLE,
      TIMER_A_OUTPUTMODE_SET_RESET, 320, // Counted up to in 10 milliseconds (.1 second) with a 32 KHz clock
      };
  Timer_A_initCompareMode(TIMER_A0_BASE, &compareParam1);

  /* Explosion timer */
  Timer_A_initCompareModeParam compareParam3 = {
        TIMER_A_CAPTURECOMPARE_REGISTER_3,
        TIMER_A_CAPTURECOMPARE_INTERRUPT_DISABLE,
        TIMER_A_OUTPUTMODE_SET_RESET, 320, // Counted up to in 10 milliseconds (.1 second) with a 32 KHz clock
        };
    Timer_A_initCompareMode(TIMER_A0_BASE, &compareParam3);

  /* Scheduler timer */
    Timer_A_initCompareModeParam compareParam2 =
    {
     TIMER_A_CAPTURECOMPARE_REGISTER_2,
     TIMER_A_CAPTURECOMPARE_INTERRUPT_ENABLE,
     TIMER_A_OUTPUTMODE_SET_RESET,
     320, //  10 milliseconds
    };
  Timer_A_initCompareMode(TIMER_A0_BASE, &compareParam2);

  Timer_A_clearTimerInterrupt(TIMER_A0_BASE);

  Timer_A_clearCaptureCompareInterrupt(TIMER_A0_BASE,
  TIMER_A_CAPTURECOMPARE_REGISTER_0 +
  TIMER_A_CAPTURECOMPARE_REGISTER_1 +
  TIMER_A_CAPTURECOMPARE_REGISTER_2 +
  TIMER_A_CAPTURECOMPARE_REGISTER_3);

  /* Scheduler timer */
//  Timer_A_initUpModeParam upModeParamA2_1 = {
//      TIMER_A_CLOCKSOURCE_ACLK,
//      TIMER_A_CLOCKSOURCE_DIVIDER_1,
//      320, // Count up to this in 10 milliseconds
//      TIMER_A_TAIE_INTERRUPT_DISABLE,
//      TIMER_A_CCIE_CCR0_INTERRUPT_ENABLE,
//      TIMER_A_DO_CLEAR,
//      false };
//  Timer_A_initUpMode(TIMER_A2_BASE, &upModeParamA2_1);
//  Timer_A_clearTimerInterrupt(TIMER_A2_BASE);
//  Timer_A_clearCaptureCompareInterrupt(TIMER_A0_BASE,
//                                       TIMER_A_CAPTURECOMPARE_REGISTER_0);

  /* Alien timer */
    Timer_A_initUpModeParam upModeParamA1_0 =
    {
     TIMER_A_CLOCKSOURCE_ACLK, // 32 KHz clock => Period is .031 milliseconds
     TIMER_A_CLOCKSOURCE_DIVIDER_1,
     3200, // 100 milliseconds
     TIMER_A_TAIE_INTERRUPT_DISABLE,
     TIMER_A_CCIE_CCR0_INTERRUPT_DISABLE,
     TIMER_A_DO_CLEAR,
     false
    };
    Timer_A_initUpMode(TIMER_A1_BASE, &upModeParamA1_0);
    Timer_A_clearTimerInterrupt(TIMER_A1_BASE);
    Timer_A_clearCaptureCompareInterrupt(TIMER_A1_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0);

    /* Bullet timer */
    Timer_A_initCompareModeParam compareParamA1_1 =
        {
         TIMER_A_CAPTURECOMPARE_REGISTER_1,
         TIMER_A_CAPTURECOMPARE_INTERRUPT_DISABLE,
         TIMER_A_OUTPUTMODE_SET_RESET,
         2*320, // 20 milliseconds
        };
    Timer_A_initCompareMode(TIMER_A1_BASE, &compareParamA1_1);
    Timer_A_clearCaptureCompareInterrupt(TIMER_A1_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_1);
//
//    Timer_A_initCompareModeParam compareParam1A1 =
//       {
//        TIMER_A_CAPTURECOMPARE_REGISTER_1,
//        TIMER_A_CAPTURECOMPARE_INTERRUPT_ENABLE,
//        TIMER_A_OUTPUTMODE_SET_RESET,
//        6400, // Counted up to in 200 milliseconds (.2 second) with a 32 KHz clock
//       };
//
//    Timer_A_initCompareMode(TIMER_A1_BASE, &compareParam1A1);
//    Timer_A_clearTimerInterrupt(TIMER_A1_BASE);
//    Timer_A_clearCaptureCompareInterrupt(TIMER_A1_BASE,
//                                        TIMER_A_CAPTURECOMPARE_REGISTER_0 +
//                                        TIMER_A_CAPTURECOMPARE_REGISTER_1);
}

void detectAndDebounceFireButtonPress(void)
{
  while(1)
  {
    if (Timer_A_getCaptureCompareInterruptStatus(TIMER_A1_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_1, TIMER_A_CAPTURECOMPARE_INTERRUPT_FLAG))
    {
        Timer_A_clearCaptureCompareInterrupt(TIMER_A1_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_1);
        /* Read the "shoot" button input */
      currentFireButtonState = GPIO_getInputPinValue(GPIO_PORT_P3, GPIO_PIN7);
      /* Debounce the button until next interrupt (50 milliseconds)
       * so wait another 50 milliseconds to see if the button is still low
       */
      if (currentFireButtonState
          == previousFireButtonState&& currentFireButtonState == GPIO_INPUT_PIN_LOW)
      {
        /* If the button is low shoot a bullet */
        /* First store the bullets starting position */
        previousBullets[currentBullet][x] = bullets[currentBullet][x];
        previousBullets[currentBullet][y] = bullets[currentBullet][y];
        /* Set the bullet to the position of the spacecraft when
         * it was fired.
         */
        bullets[currentBullet][x] = spacecraftPosition[x];
        /* Subtract from the spacecraft position so the bullet looks like it is ahead of the
         * spacecraft when we shoot it and also we don't
         * clear the spacecraft when we clear the bullet
         */
        bullets[currentBullet][y] = spacecraftPosition[y] - 5;
        /* Advance to the next bullet or back to 0 if there isn't
         * another bullet
         */
        if (currentBullet < NUM_BULLETS)
        {
          currentBullet++;
        }
        else
        {
          currentBullet = 0;
        }
        /* Reset previous button state to be ready for the next button press */
        previousFireButtonState = GPIO_INPUT_PIN_HIGH;

        /* Generate PWM for the buzzer */
        Timer_A_outputPWMParam pwmBuzzerParam1 = {
            TIMER_A_CLOCKSOURCE_SMCLK,
            TIMER_A_CLOCKSOURCE_DIVIDER_1,
            60000,
            TIMER_A_CAPTURECOMPARE_REGISTER_2,
            TIMER_A_OUTPUTMODE_RESET_SET, 60000/2
            };
        Timer_A_outputPWM(TIMER_A2_BASE, &pwmBuzzerParam1);
        pwmFireCounter = 0;
      }
      else
      {
        previousFireButtonState = currentFireButtonState;
        /* Let the pwm signal to the buzzer run for 3 interrupts */
        if (pwmFireCounter < 3)
        {
          pwmFireCounter += 1;
        }
        else if (pwmFireCounter == 3)
        {
          Timer_A_stop(TIMER_A2_BASE);
          pwmFireCounter = 4;
        }

        /* If we aren't shooting and we can make the explosion noise */
        if (pwmExplosionCounter < 3 && pwmFireCounter == 4)
        {
          pwmExplosionCounter += 1;
        }
        /* If we aren't shooting we can turn off the timer and we are done with the explosion noises */
        else if (pwmFireCounter == 4 && pwmExplosionCounter == 3)
        {
          Timer_A_stop(TIMER_A2_BASE);
          pwmExplosionCounter = 4;
        }

      }

      /* Advance the other bullets by clearing their previous positions
       * and drawing them at their new positions
       */
      uint8_t i;
      for (i = 0; i < NUM_BULLETS ; i++)
      {
        /* If the bullets have been shot which means they aren't the
         * initial values they were set to draw them on the screen
         */
        if (bullets[i][x] != 200 && bullets[i][y] != 200)
        {
          /* Clear the bullets previous position */
          bulletsRect.xMax = previousBullets[i][x] + 2;
          bulletsRect.xMin = previousBullets[i][x] - 2;
          bulletsRect.yMax = previousBullets[i][y] + 2;
          bulletsRect.yMin = previousBullets[i][y] - 2;
          Graphics_fillRectangleOnDisplay(g_sContext.display, &bulletsRect,
                                          g_sContext.background);
          /* If the bullet has moved off the screen reset
           * it.
           */
          if (bullets[i][y] <= 0)
          {
            bullets[i][x] = 200;
            bullets[i][y] = 200;

          }
          else
          {
            /* Store the bullets position before moving to the next one */
            previousBullets[i][x] = bullets[i][x];
            previousBullets[i][y] = bullets[i][y];
            /* Show the bullets moving up to the top of the screen */
            bullets[i][y]--;

            bulletsRect.xMax = bullets[i][x] + 2;
            bulletsRect.xMin = bullets[i][x] - 2;
            bulletsRect.yMax = bullets[i][y] + 2;
            bulletsRect.yMin = bullets[i][y] - 2;
            Graphics_fillRectangle(&g_sContext, &bulletsRect);
            /* Check if any of the aliens have been hit
             * by the bullet
             */
            uint8_t j;
            for (j = 0; j < NUM_ALIENS ; j++)
            {
              alienRect.xMax = aliens[j][x] + 2;
              alienRect.xMin = aliens[j][x] - 2;
              alienRect.yMax = aliens[j][y] + 2;
              alienRect.yMin = aliens[j][y] - 2;

              /* If a bullet and alien intersect... */
              if (Graphics_isRectangleOverlap(&bulletsRect, &alienRect))
              {
                /* Draw an explosion at the alien and bullet  */
                explosions[j][x] = aliens[j][x];
                explosions[j][y] = aliens[j][y];
                explosions[j][r] = 5;
                explosions[j][s] = 1;
                explosions[j][d] = 1;
                /* Generate PWM for the buzzer to make a sound of an explosion */
                Timer_A_outputPWMParam pwmBuzzerParam1 = {
                    TIMER_A_CLOCKSOURCE_SMCLK,
                    TIMER_A_CLOCKSOURCE_DIVIDER_1,
                    3200,
                    TIMER_A_CAPTURECOMPARE_REGISTER_2,
                    TIMER_A_OUTPUTMODE_RESET_SET, 1600 // 50% duty cycle
                    };
                Timer_A_outputPWM(TIMER_A2_BASE, &pwmBuzzerParam1);
                pwmExplosionCounter = 0;
                /* Clear the aliens previous position */
                alienRect.xMax = aliens[j][x] + 2;
                alienRect.xMin = aliens[j][x] - 2;
                alienRect.yMax = aliens[j][y] + 2;
                alienRect.yMin = aliens[j][y] - 2;
                Graphics_fillRectangleOnDisplay(g_sContext.display, &alienRect,
                                                g_sContext.background);
                /* Kill the alien */
                aliens[j][x] = 200;
                aliens[j][y] = 200;

                /* Clear the bullet */
                /* Clear the bullets position */
                bulletsRect.xMax = bullets[i][x] + 2;
                bulletsRect.xMin = bullets[i][x] - 2;
                bulletsRect.yMax = bullets[i][y] + 2;
                bulletsRect.yMin = bullets[i][y] - 2;
                Graphics_fillRectangleOnDisplay(g_sContext.display, &bulletsRect,
                                                g_sContext.background);

                /* Set the bullet to its starting values
                 * so it is not redrawn again
                 */
                bullets[i][x] = 200;
                bullets[i][y] = 200;
                previousBullets[i][x] = bullets[i][x];
                previousBullets[i][y] = bullets[i][y];
              }
            }
          }
        }
      }
    }
  }
}

void drawExplosions(void)
{
  uint8_t i;
  while (1)
  {
    if (Timer_A_getCaptureCompareInterruptStatus(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_3, TIMER_A_CAPTURECOMPARE_INTERRUPT_FLAG))
    {
      Timer_A_clearCaptureCompareInterrupt(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_3);
      /* Advance the explosions */
      for (i = 0; i < NUM_EXPLOSIONS ; i++)
      {
        /* If the explosion has been set to be shown */
        if (explosions[i][s])
        {
          /* If the explosion is set to increase in direction */
          if (explosions[i][d] == 1)
          {
            /* Don't need to clear previous circle because the explosion gets bigger */
            wait(&sem1, &semC3);
            Graphics_fillCircle(&g_sContext, explosions[i][x], explosions[i][y],
                                explosions[i][r]);
            signal(&sem1, &semC3);
            explosions[i][r] += 5;
            /* Max explosion radius should be 10 but we have to go to 15 to draw a circle with radius 10 */
            if (explosions[i][r] >= 15)
            {
              explosions[i][d] = 0;
              explosions[i][r] = 10;
            }
          }
          else
          {
            /* Clear previous explosion circle because circle gets smaller */
            explosionRect.xMax = explosions[i][x] + explosions[i][r];
            explosionRect.xMin = explosions[i][x] - explosions[i][r];
            explosionRect.yMax = explosions[i][y] + explosions[i][r];
            explosionRect.yMin = explosions[i][y] - explosions[i][r];
            wait(&sem1, &semC3);
            Graphics_fillRectangleOnDisplay(g_sContext.display, &explosionRect,
                                            g_sContext.background);
            signal(&sem1, &semC3);
            /* Decrease the explosion size */
            explosions[i][r] -= 5;
            /* Minimum explosion size is 0.  Stop the explosion
             * when it gets to that size
             */
            if (explosions[i][r] <= 0)
            {
              /* Don't draw circle with radius 0 */
              explosions[i][s] = 0;
            }
            else
            {
              /* Otherwise keep drawing smaller explosions */
              wait(&sem1, &semC3);
              Graphics_fillCircle(&g_sContext, explosions[i][x], explosions[i][y],
                                  explosions[i][r]);
              signal(&sem1, &semC3);
            }
          }
        }
      }
    }
  }
}

void drawAliens(void)
{
  /* Draw the aliens */
  uint8_t i;
  while (1)
  {
    if (Timer_A_getCaptureCompareInterruptStatus(TIMER_A1_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0, TIMER_A_CAPTURECOMPARE_INTERRUPT_FLAG))
    {
      Timer_A_clearCaptureCompareInterrupt(TIMER_A1_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0);
      for (i = 0; i < NUM_ALIENS ; i++)
      {
        /* If the aliens aren't dead... */
        if (aliens[i][x] != 200 && aliens[i][y] != 200)
        {
          wait(&sem1, &semC2);
          /* Clear the aliens previous position */
          alienRect.xMax = previousAliens[i][x] + 2;
          alienRect.xMin = previousAliens[i][x] - 2;
          alienRect.yMax = previousAliens[i][y] + 2;
          alienRect.yMin = previousAliens[i][y] - 2;
          Graphics_fillRectangleOnDisplay(g_sContext.display, &alienRect,
                                          g_sContext.background);
          /* Keep the aliens on the screen */
          /* Alien is moving to the right almost off the screen */
          if (aliens[i][x] > 125 && previousAliens[i][x] < aliens[i][x])
          {
            previousAliens[i][x] = aliens[i][x];
            aliens[i][x]--;
          }
          /* Alien is moving to the left almost off the screen */
          else if (aliens[i][x] < 2 && previousAliens[i][x] > aliens[i][x])
          {
            previousAliens[i][x] = aliens[i][x];
            aliens[i][x]++;
          }
          /* Alien is just moving to the right */
          else if (aliens[i][x] > previousAliens[i][x])
          {
            /* Keep alien moving to the right */
            previousAliens[i][x] = aliens[i][x];
            aliens[i][x]++;
          }
          /* Alien is just moving to the left */
          else if (aliens[i][x] < previousAliens[i][x])
          {
            /* Keep alien moving to the left */
            previousAliens[i][x] = aliens[i][x];
            aliens[i][x]--;
          }
          /* Alien hasn't started moving */
          else if (aliens[i][x] == previousAliens[i][x])
          {
            /* Choose a direction based on i */
            if (i % 2)
            {
              previousAliens[i][x] = aliens[i][x];
              aliens[i][x]++;
            }
            else
            {
              previousAliens[i][x] = aliens[i][x];
              aliens[i][x]--;
            }
          }
          else
          {
            // This shouldn't be reached.
          }
          /* Draw the alien at its new position */
          alienRect.xMax = aliens[i][x] + 2;
          alienRect.xMin = aliens[i][x] - 2;
          alienRect.yMax = aliens[i][y] + 2;
          alienRect.yMin = aliens[i][y] - 2;
          Graphics_fillRectangle(&g_sContext, &alienRect);
          signal(&sem1, &semC2);
        }
        else
        {
          /* Don't draw the alien because it is dead */
        }
      }
    }
  }
}

void drawSpacecraft(void)
{
  while (1)
  {
    if (ADC12_A_getInterruptStatus(ADC12_A_BASE, ADC12_A_IFG1))
    {
      /* Store the spacecrafts previous position */
      previousSpacecraftPosition[x] = spacecraftPosition[x];
      previousSpacecraftPosition[y] = spacecraftPosition[y];
      /* Store the spacecrafts new position */
      spacecraftPosition[x] = mapValToRange(
          (uint32_t) ADC12_A_getResults(ADC12_A_BASE, ADC12_A_MEMORY_0), 0UL,
          255UL, 0UL, 127UL);
      /* Invert the y values so when the joystick goes up y goes down */
      spacecraftPosition[y] = 0x7F
          & ~mapValToRange(
              (uint32_t) ADC12_A_getResults(ADC12_A_BASE, ADC12_A_MEMORY_1),
              0UL, 255UL, 0UL, 127UL);
      /* If the spacecraft is in a new position clear its old position and redraw it */
      if (previousSpacecraftPosition[x] != spacecraftPosition[x]
          || previousSpacecraftPosition[y] != spacecraftPosition[y])
      {
        wait(&sem1, &semC1);
        /* Clear the spacecrafts previous position */
        spacecraftRect.xMax = previousSpacecraftPosition[x] + 2;
        spacecraftRect.xMin = previousSpacecraftPosition[x] - 2;
        spacecraftRect.yMax = previousSpacecraftPosition[y] + 2;
        spacecraftRect.yMin = previousSpacecraftPosition[y] - 2;
        Graphics_fillRectangleOnDisplay(g_sContext.display, &spacecraftRect,
                                        g_sContext.background);
        /* Draw it's new position */
        spacecraftRect.xMax = spacecraftPosition[x] + 2;
        spacecraftRect.xMin = spacecraftPosition[x] - 2;
        spacecraftRect.yMax = spacecraftPosition[y] + 2;
        spacecraftRect.yMin = spacecraftPosition[y] - 2;
        Graphics_fillRectangle(&g_sContext, &spacecraftRect);
        signal(&sem1, &semC1);
      }
    }
  }
}

#pragma vector=TIMER0_A1_VECTOR
__interrupt void TIMER0_A1_ISR(void)
{
  switch (__even_in_range(TA0IV, 4))
    {
      case 4:
      {
        /* Undo the compiler register pushing */
        __asm(" popm.a #1, r15");
        if (currentRunningTask == NUM_TASKS)
        {
          /* Don't do anything because this is the first task to run so it still has registers on its stack
           * since that is how the task stack was initialized */
        }
        else
        {
          /* Push the general purpose registers R4 - R15 onto the stack saving the context of the current task */
          __asm(" push.a #12, r15");
          /* Save the stack pointer of the executing task */
          __asm(" mov.a sp, tempSP");
          stack_pointer[currentRunningTask] = tempSP;
        }
        /* Switch the task to the next one by putting the stack pointer of the next task into the Stack Pointer register */
        /* Use round robin scheduling when choosing tasks */
        if (currentRunningTask < (NUM_TASKS - 1))
        {
          currentRunningTask++;
        }
        else
        {
          currentRunningTask = 0;
        }
        tempSP = stack_pointer[currentRunningTask];
        __asm(" mov.a tempSP, sp");
        __asm(" popm.a #12, r15");
        __asm(" reti");
      }
    }
}

//#pragma vector=ADC12_VECTOR
//__interrupt void ADC12_ISR(void)
//{
//    switch (__even_in_range(ADC12IV, 8))
//    {
//        case  8: //Vector  8:  ADC12IFG1
//            /* Store the spacecrafts previous position */
//            previousSpacecraftPosition[x] =  spacecraftPosition[x];
//            previousSpacecraftPosition[y] =  spacecraftPosition[y];
//            /* Store the spacecrafts new position */
//            spacecraftPosition[x] = mapValToRange((uint32_t)ADC12_A_getResults(ADC12_A_BASE, ADC12_A_MEMORY_0), 0UL, 255UL, 0UL, 127UL);
//            /* Invert the y values so when the joystick goes up y goes down */
//            spacecraftPosition[y] = 0x7F & ~mapValToRange((uint32_t)ADC12_A_getResults(ADC12_A_BASE, ADC12_A_MEMORY_1), 0UL, 255UL, 0UL, 127UL);
//            /* If the spacecraft is in a new position clear its old position and redraw it */
//              if (previousSpacecraftPosition[x] != spacecraftPosition[x] || previousSpacecraftPosition[y] != spacecraftPosition[y])
//              {
//                /* Clear the spacecrafts previous position */
//                  spacecraftRect.xMax = previousSpacecraftPosition[x] + 2;
//                  spacecraftRect.xMin = previousSpacecraftPosition[x] - 2;
//                  spacecraftRect.yMax = previousSpacecraftPosition[y] + 2;
//                  spacecraftRect.yMin = previousSpacecraftPosition[y] - 2;
//                Graphics_fillRectangleOnDisplay(g_sContext.display, &spacecraftRect, g_sContext.background);
//                /* Draw it's new position */
//                spacecraftRect.xMax = spacecraftPosition[x] + 2;
//                spacecraftRect.xMin = spacecraftPosition[x] - 2;
//                spacecraftRect.yMax = spacecraftPosition[y] + 2;
//                spacecraftRect.yMin = spacecraftPosition[y] - 2;
//                Graphics_fillRectangle(&g_sContext, &spacecraftRect);
//              }
//            break;
//        default: break;
//    }
//}

//#pragma vector=TIMER1_A1_VECTOR
//__interrupt void TIMER1_A1_ISR(void)
//{
//  switch (__even_in_range(TA1IV, 4))
//  {
//  case 2: // CCR1 IFG
//  {
//    /* Draw the aliens */
//    uint8_t i;
//    for (i = 0; i < NUM_ALIENS ; i++)
//    {
//      /* If the aliens aren't dead... */
//      if (aliens[i][x] != 200 && aliens[i][y] != 200)
//      {
//        /* Clear the aliens previous position */
//        alienRect.xMax = previousAliens[i][x] + 2;
//        alienRect.xMin = previousAliens[i][x] - 2;
//        alienRect.yMax = previousAliens[i][y] + 2;
//        alienRect.yMin = previousAliens[i][y] - 2;
//        Graphics_fillRectangleOnDisplay(g_sContext.display, &alienRect,
//                                        g_sContext.background);
//
//        /* Keep the aliens on the screen */
//        /* Alien is moving to the right almost off the screen */
//        if (aliens[i][x] > 125 && previousAliens[i][x] < aliens[i][x])
//        {
//          previousAliens[i][x] = aliens[i][x];
//          aliens[i][x]--;
//        }
//        /* Alien is moving to the left almost off the screen */
//        else if (aliens[i][x] < 2 && previousAliens[i][x] > aliens[i][x])
//        {
//          previousAliens[i][x] = aliens[i][x];
//          aliens[i][x]++;
//        }
//        /* Alien is just moving to the right */
//        else if (aliens[i][x] > previousAliens[i][x])
//        {
//          /* Keep alien moving to the right */
//          previousAliens[i][x] = aliens[i][x];
//          aliens[i][x]++;
//        }
//        /* Alien is just moving to the left */
//        else if (aliens[i][x] < previousAliens[i][x])
//        {
//          /* Keep alien moving to the left */
//          previousAliens[i][x] = aliens[i][x];
//          aliens[i][x]--;
//        }
//        /* Alien hasn't started moving */
//        else if (aliens[i][x] == previousAliens[i][x])
//        {
//          /* Choose a direction based on i */
//          if (i % 2)
//          {
//            previousAliens[i][x] = aliens[i][x];
//            aliens[i][x]++;
//          }
//          else
//          {
//            previousAliens[i][x] = aliens[i][x];
//            aliens[i][x]--;
//          }
//        }
//        else
//        {
//          printf("[Alien] Movement should not be reached");
//        }
//        /* Draw the alien at its new position */
//        alienRect.xMax = aliens[i][x] + 2;
//        alienRect.xMin = aliens[i][x] - 2;
//        alienRect.yMax = aliens[i][y] + 2;
//        alienRect.yMin = aliens[i][y] - 2;
//        Graphics_fillRectangle(&g_sContext, &alienRect);
//      }
//      else
//      {
//        /* Don't draw the alien because it is dead */
//      }
//    }
//    /* Advance the explosions */
//    for (i = 0; i < NUM_ALIENS ; i++)
//    {
//      /* If the explosion has been set to be shown */
//      if (explosions[i][s])
//      {
//        /* If the explosion is set to increase in direction */
//        if (explosions[i][d] == 1)
//        {
//          /* Don't need to clear previous circle because the explosion gets bigger */
//          Graphics_fillCircle(&g_sContext, explosions[i][x], explosions[i][y],
//                              explosions[i][r]);
//          explosions[i][r] += 5;
//          /* Max explosion radius should be 10 but we have to go to 15 to draw a circle with radius 10 */
//          if (explosions[i][r] >= 15)
//          {
//            explosions[i][d] = 0;
//            explosions[i][r] = 10;
//          }
//        }
//        else
//        {
//          /* Clear previous explosion circle because circle gets smaller */
//          explosionRect.xMax = explosions[i][x] + explosions[i][r];
//          explosionRect.xMin = explosions[i][x] - explosions[i][r];
//          explosionRect.yMax = explosions[i][y] + explosions[i][r];
//          explosionRect.yMin = explosions[i][y] - explosions[i][r];
//          Graphics_fillRectangleOnDisplay(g_sContext.display, &explosionRect,
//                                          g_sContext.background);
//          /* Decrease the explosion size */
//          explosions[i][r] -= 5;
//          /* Minimum explosion size is 0.  Stop the explosion
//           * when it gets to that size
//           */
//          if (explosions[i][r] <= 0)
//          {
//            /* Don't draw circle with radius 0 */
//            explosions[i][s] = 0;
//          }
//          else
//          {
//            /* Otherwise keep drawing smaller explosions */
//            Graphics_fillCircle(&g_sContext, explosions[i][x], explosions[i][y],
//                                explosions[i][r]);
//          }
//        }
//      }
//    }
//    break;
//  }
//  default:
//    break;
//  }
//}

//#pragma vector=TIMER0_A1_VECTOR
//__interrupt void TIMER0_A1_ISR(void)
//{
//  switch (__even_in_range(TA0IV, 4))
//  {
//  case 4: // CCR2 IFG
//    /* Read the "shoot" button input */
//    currentFireButtonState = GPIO_getInputPinValue(GPIO_PORT_P3, GPIO_PIN7);
//    /* Debounce the button until next interrupt (50 milliseconds)
//     * so wait another 50 milliseconds to see if the button is still low
//     */
//    if (currentFireButtonState
//        == previousFireButtonState&& currentFireButtonState == GPIO_INPUT_PIN_LOW)
//    {
//      /* If the button is low shoot a bullet */
//      /* First store the bullets starting position */
//      previousBullets[currentBullet][x] = bullets[currentBullet][x];
//      previousBullets[currentBullet][y] = bullets[currentBullet][y];
//      /* Set the bullet to the position of the spacecraft when
//       * it was fired.
//       */
//      bullets[currentBullet][x] = spacecraftPosition[x];
//      /* Subtract from the spacecraft position so the bullet looks like it is ahead of the
//       * spacecraft when we shoot it and also we don't
//       * clear the spacecraft when we clear the bullet
//       */
//      bullets[currentBullet][y] = spacecraftPosition[y] - 5;
//      /* Advance to the next bullet or back to 0 if there isn't
//       * another bullet
//       */
//      if (currentBullet < NUM_BULLETS)
//      {
//        currentBullet++;
//      }
//      else
//      {
//        currentBullet = 0;
//      }
//      /* Reset previous button state to be ready for the next button press */
//      previousFireButtonState = GPIO_INPUT_PIN_HIGH;
//
//      /* Generate PWM for the buzzer */
//      Timer_A_outputPWMParam pwmBuzzerParam1 = {
//          TIMER_A_CLOCKSOURCE_ACLK,
//          TIMER_A_CLOCKSOURCE_DIVIDER_1,
//          3200,
//          TIMER_A_CAPTURECOMPARE_REGISTER_2,
//          TIMER_A_OUTPUTMODE_RESET_SET, 320 // 10% duty cycle
//          };
//      Timer_A_outputPWM(TIMER_A2_BASE, &pwmBuzzerParam1);
//      pwmFireCounter = 0;
//    }
//    else
//    {
//      previousFireButtonState = currentFireButtonState;
//      /* Let the pwm signal to the buzzer run for 3 interrupts */
//      if (pwmFireCounter < 3)
//      {
//        pwmFireCounter += 1;
//      }
//      else if (pwmFireCounter == 3)
//      {
//        Timer_A_stop(TIMER_A2_BASE);
//        pwmFireCounter = 4;
//      }
//
//      /* If we aren't shooting and we can make the explosion noise */
//      if (pwmExplosionCounter < 3 && pwmFireCounter == 4)
//      {
//        pwmExplosionCounter += 1;
//      }
//      /* If we aren't shooting we can turn off the timer and we are done with the explosion noises */
//      else if (pwmFireCounter == 4 && pwmExplosionCounter == 3)
//      {
//        Timer_A_stop(TIMER_A2_BASE);
//        pwmExplosionCounter = 4;
//      }
//
//    }
//
//    /* Advance the other bullets by clearing their previous positions
//     * and drawing them at their new positions
//     */
//    uint8_t i;
//    for (i = 0; i < NUM_BULLETS ; i++)
//    {
//      /* If the bullets have been shot which means they aren't the
//       * initial values they were set to draw them on the screen
//       */
//      if (bullets[i][x] != 200 && bullets[i][y] != 200)
//      {
//        /* Clear the bullets previous position */
//        bulletsRect.xMax = previousBullets[i][x] + 2;
//        bulletsRect.xMin = previousBullets[i][x] - 2;
//        bulletsRect.yMax = previousBullets[i][y] + 2;
//        bulletsRect.yMin = previousBullets[i][y] - 2;
//        Graphics_fillRectangleOnDisplay(g_sContext.display, &bulletsRect,
//                                        g_sContext.background);
//        /* If the bullet has moved off the screen reset
//         * it.
//         */
//        if (bullets[i][y] <= 0)
//        {
//          bullets[i][x] = 200;
//          bullets[i][y] = 200;
//
//        }
//        else
//        {
//          /* Store the bullets position before moving to the next one */
//          previousBullets[i][x] = bullets[i][x];
//          previousBullets[i][y] = bullets[i][y];
//          /* Show the bullets moving up to the top of the screen */
//          bullets[i][y]--;
//
//          bulletsRect.xMax = bullets[i][x] + 2;
//          bulletsRect.xMin = bullets[i][x] - 2;
//          bulletsRect.yMax = bullets[i][y] + 2;
//          bulletsRect.yMin = bullets[i][y] - 2;
//          Graphics_fillRectangle(&g_sContext, &bulletsRect);
//          /* Check if any of the aliens have been hit
//           * by the bullet
//           */
//          uint8_t j;
//          for (j = 0; j < NUM_ALIENS ; j++)
//          {
//            alienRect.xMax = aliens[j][x] + 2;
//            alienRect.xMin = aliens[j][x] - 2;
//            alienRect.yMax = aliens[j][y] + 2;
//            alienRect.yMin = aliens[j][y] - 2;
//
//            /* If a bullet and alien intersect... */
//            if (Graphics_isRectangleOverlap(&bulletsRect, &alienRect))
//            {
//              /* Draw an explosion at the alien and bullet  */
//              explosions[j][x] = aliens[j][x];
//              explosions[j][y] = aliens[j][y];
//              explosions[j][r] = 5;
//              explosions[j][s] = 1;
//              explosions[j][d] = 1;
//              /* Generate PWM for the buzzer to make a sound of an explosion */
//              Timer_A_outputPWMParam pwmBuzzerParam1 = {
//                  TIMER_A_CLOCKSOURCE_ACLK,
//                  TIMER_A_CLOCKSOURCE_DIVIDER_1,
//                  3200,
//                  TIMER_A_CAPTURECOMPARE_REGISTER_2,
//                  TIMER_A_OUTPUTMODE_RESET_SET, 1600 // 50% duty cycle
//                  };
//              Timer_A_outputPWM(TIMER_A2_BASE, &pwmBuzzerParam1);
//              pwmExplosionCounter = 0;
//              /* Clear the aliens previous position */
//              alienRect.xMax = aliens[j][x] + 2;
//              alienRect.xMin = aliens[j][x] - 2;
//              alienRect.yMax = aliens[j][y] + 2;
//              alienRect.yMin = aliens[j][y] - 2;
//              Graphics_fillRectangleOnDisplay(g_sContext.display, &alienRect,
//                                              g_sContext.background);
//              /* Kill the alien */
//              aliens[j][x] = 200;
//              aliens[j][y] = 200;
//
//              /* Clear the bullet */
//              /* Clear the bullets position */
//              bulletsRect.xMax = bullets[i][x] + 2;
//              bulletsRect.xMin = bullets[i][x] - 2;
//              bulletsRect.yMax = bullets[i][y] + 2;
//              bulletsRect.yMin = bullets[i][y] - 2;
//              Graphics_fillRectangleOnDisplay(g_sContext.display, &bulletsRect,
//                                              g_sContext.background);
//
//              /* Set the bullet to its starting values
//               * so it is not redrawn again
//               */
//              bullets[i][x] = 200;
//              bullets[i][y] = 200;
//              previousBullets[i][x] = bullets[i][x];
//              previousBullets[i][y] = bullets[i][y];
//            }
//          }
//        }
//      }
//    }
//    break;
//  default:
//    break;
//  }
//}
