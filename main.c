#include <msp430.h>
#include <driverlib.h>
#include <grlib.h>
#include "Crystalfontz128x128_ST7735.h"
#include <stdio.h>

void initWatchdog(void);
void initClocks(void);
void initTimers(void);
void initGPIO(void);
void initJoystickADC(void);
void initObjects(void);
uint32_t mapValToRange(uint32_t x, uint32_t input_min, uint32_t input_max, uint32_t output_min, uint32_t output_max);

#define MCLK_SMCLK_DESIRED_FREQUENCY_IN_KHZ 25000

uint8_t pwmFireCounter = 4;
uint8_t pwmExplosionCounter = 4;

/* Graphic library context */
Graphics_Context g_sContext;

/* ADC results buffer */
static uint32_t spacecraftPosition[2];
static uint32_t previousSpacecraftPosition[2];

Graphics_Rectangle spacecraftRect;

Graphics_Rectangle bulletsRect;

Graphics_Rectangle alienRect;

Graphics_Rectangle explosionRect;

/* Fire button S2  value */
uint8_t currentFireButtonState;
uint8_t previousFireButtonState;

enum
{
    x = 0,
    y = 1,
    s = 2, // Enable or not
    r = 3, // Radius
    d = 4  // Direction
};

/* Aliens */
#define NUM_ALIENS (uint8_t)3
uint16_t aliens[NUM_ALIENS][2];
uint16_t previousAliens[NUM_ALIENS][2];

uint16_t explosions[NUM_ALIENS][5];

/* Spacecraft bullets */
#define NUM_BULLETS (uint8_t)10
uint16_t bullets[NUM_BULLETS][2];
uint16_t previousBullets[NUM_BULLETS][2];
uint8_t currentBullet = 0;

/**
 * main.c
 */
void main(void)
{
    //uint8_t i;

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

    // Globally enable interrupts
    __bis_SR_register(GIE);

    // Enable/Start first sampling and conversion cycle
    ADC12_A_startConversion(ADC12_A_BASE,
                ADC12_A_MEMORY_0,
                ADC12_A_REPEATED_SEQOFCHANNELS);

    // Start the timer to trigger the Joystick ADC
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
    WDT_A_hold( WDT_A_BASE );
}

void initObjects(void)
{
    uint8_t i;
    for (i = 0; i < NUM_BULLETS; i++)
    {
        bullets[i][x] = 200;
        bullets[i][y] = 200;
        previousBullets[i][x] = bullets[i][x];
        previousBullets[i][y] = bullets[i][y];
    }

    for (i = 0; i < NUM_ALIENS; i++)
    {
        aliens[i][x] = 2+10*i;
        aliens[i][y] = 2+10*i;
        previousAliens[i][x] = aliens[i][x];
        previousAliens[i][y] = aliens[i][y];

        /* Disable the explosions */
        explosions[i][s] = 0;

    }

    spacecraftPosition[x] = 0;
    spacecraftPosition[y] = 0;

    previousSpacecraftPosition[x] =  spacecraftPosition[x];
    previousSpacecraftPosition[y] =  spacecraftPosition[y];
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
    UCS_initFLLSettle(MCLK_SMCLK_DESIRED_FREQUENCY_IN_KHZ,
                      (MCLK_SMCLK_DESIRED_FREQUENCY_IN_KHZ/(UCS_REFOCLK_FREQUENCY/1024)));
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
    ADC12_A_configureMemoryParam param0 =
    {
     ADC12_A_MEMORY_0,
     ADC12_A_INPUT_A5,
     ADC12_A_VREFPOS_AVCC,
     ADC12_A_VREFNEG_AVSS,
     ADC12_A_NOTENDOFSEQUENCE
    };

    ADC12_A_configureMemory(ADC12_A_BASE, &param0);

    /* Joystick Y memory input */
    ADC12_A_configureMemoryParam param1 =
       {
        ADC12_A_MEMORY_1,
        ADC12_A_INPUT_A3,
        ADC12_A_VREFPOS_AVCC,
        ADC12_A_VREFNEG_AVSS,
        ADC12_A_ENDOFSEQUENCE
       };

       ADC12_A_configureMemory(ADC12_A_BASE, &param1);

    // Enable memory buffer 1 interrupts
    // so interrupt fires when sequence mem0 - mem1 or x - y is completed
    ADC12_A_clearInterrupt(ADC12_A_BASE,
        ADC12IFG1);
    ADC12_A_enableInterrupt(ADC12_A_BASE,
        ADC12IE1);
}

// https://stackoverflow.com/questions/5731863/mapping-a-numeric-range-onto-another
// https://github.com/arduino/Arduino/issues/2466
uint32_t mapValToRange(uint32_t x, uint32_t input_min, uint32_t input_max, uint32_t output_min, uint32_t output_max)
{
    uint32_t val = (uint32_t)(((x - input_min)*(output_max - output_min + 1)/(input_max - input_min + 1)) + output_min);
    return val;
}

void initTimers(void)
{

    /* Joystick ADC timer */
    Timer_A_initUpModeParam upModeParam=
        {
         TIMER_A_CLOCKSOURCE_ACLK, // 32 KHz clock => Period is .031 milliseconds
         TIMER_A_CLOCKSOURCE_DIVIDER_1,
         3200, // This needs to be the same value as the compare.  I think it has to do with the the set reset triggers.
         TIMER_A_TAIE_INTERRUPT_DISABLE,
         TIMER_A_CCIE_CCR0_INTERRUPT_DISABLE,
         TIMER_A_DO_CLEAR,
         false
        };

    Timer_A_initUpMode(TIMER_A0_BASE, &upModeParam);

    Timer_A_initCompareModeParam compareParam1 =
    {
     TIMER_A_CAPTURECOMPARE_REGISTER_1,
     TIMER_A_CAPTURECOMPARE_INTERRUPT_DISABLE,
     TIMER_A_OUTPUTMODE_SET_RESET,
     3200, // Counted up to in 100 milliseconds (.1 second) with a 32 KHz clock
    };

    Timer_A_initCompareModeParam compareParam2 =
    {
     TIMER_A_CAPTURECOMPARE_REGISTER_2,
     TIMER_A_CAPTURECOMPARE_INTERRUPT_ENABLE,
     TIMER_A_OUTPUTMODE_OUTBITVALUE,
     1600, // Counted up to in 50 milliseconds (.05 second) with a 32 KHz clock
    };

    Timer_A_initCompareMode(TIMER_A0_BASE, &compareParam1);
    Timer_A_initCompareMode(TIMER_A0_BASE, &compareParam2);

    Timer_A_clearTimerInterrupt(TIMER_A0_BASE);

    Timer_A_clearCaptureCompareInterrupt(TIMER_A0_BASE,
                                         TIMER_A_CAPTURECOMPARE_REGISTER_0 +
                                         TIMER_A_CAPTURECOMPARE_REGISTER_1 +
                                         TIMER_A_CAPTURECOMPARE_REGISTER_2);

    /* Alien timer */
    Timer_A_initUpModeParam upModeParamA1=
    {
     TIMER_A_CLOCKSOURCE_ACLK, // 32 KHz clock => Period is .031 milliseconds
     TIMER_A_CLOCKSOURCE_DIVIDER_1,
     6400,
     TIMER_A_TAIE_INTERRUPT_DISABLE,
     TIMER_A_CCIE_CCR0_INTERRUPT_DISABLE,
     TIMER_A_DO_CLEAR,
     false
    };

    Timer_A_initUpMode(TIMER_A1_BASE, &upModeParamA1);

    Timer_A_initCompareModeParam compareParam1A1 =
       {
        TIMER_A_CAPTURECOMPARE_REGISTER_1,
        TIMER_A_CAPTURECOMPARE_INTERRUPT_ENABLE,
        TIMER_A_OUTPUTMODE_SET_RESET,
        6400, // Counted up to in 200 milliseconds (.2 second) with a 32 KHz clock
       };

    Timer_A_initCompareMode(TIMER_A1_BASE, &compareParam1A1);
    Timer_A_clearTimerInterrupt(TIMER_A1_BASE);
    Timer_A_clearCaptureCompareInterrupt(TIMER_A1_BASE,
                                        TIMER_A_CAPTURECOMPARE_REGISTER_0 +
                                        TIMER_A_CAPTURECOMPARE_REGISTER_1);
}

#pragma vector=ADC12_VECTOR
__interrupt void ADC12_ISR(void)
{
    switch (__even_in_range(ADC12IV, 8))
    {
        case  8: //Vector  8:  ADC12IFG1
            /* Store the spacecrafts previous position */
            previousSpacecraftPosition[x] =  spacecraftPosition[x];
            previousSpacecraftPosition[y] =  spacecraftPosition[y];
            /* Store the spacecrafts new position */
            spacecraftPosition[x] = mapValToRange((uint32_t)ADC12_A_getResults(ADC12_A_BASE, ADC12_A_MEMORY_0), 0UL, 255UL, 0UL, 127UL);
            /* Invert the y values so when the joystick goes up y goes down */
            spacecraftPosition[y] = 0x7F & ~mapValToRange((uint32_t)ADC12_A_getResults(ADC12_A_BASE, ADC12_A_MEMORY_1), 0UL, 255UL, 0UL, 127UL);
            /* If the spacecraft is in a new position clear its old position and redraw it */
              if (previousSpacecraftPosition[x] != spacecraftPosition[x] || previousSpacecraftPosition[y] != spacecraftPosition[y])
              {
                /* Clear the spacecrafts previous position */
                  spacecraftRect.xMax = previousSpacecraftPosition[x] + 2;
                  spacecraftRect.xMin = previousSpacecraftPosition[x] - 2;
                  spacecraftRect.yMax = previousSpacecraftPosition[y] + 2;
                  spacecraftRect.yMin = previousSpacecraftPosition[y] - 2;
                Graphics_fillRectangleOnDisplay(g_sContext.display, &spacecraftRect, g_sContext.background);
                /* Draw it's new position */
                spacecraftRect.xMax = spacecraftPosition[x] + 2;
                spacecraftRect.xMin = spacecraftPosition[x] - 2;
                spacecraftRect.yMax = spacecraftPosition[y] + 2;
                spacecraftRect.yMin = spacecraftPosition[y] - 2;
                Graphics_fillRectangle(&g_sContext, &spacecraftRect);
              }
            break;
        default: break;
    }
}

#pragma vector=TIMER0_A1_VECTOR
__interrupt void TIMER0_A1_ISR(void)
{
  switch (__even_in_range(TA0IV, 4))
  {
      case 4: // CCR2 IFG
          /* Read the "shoot" button input */
          currentFireButtonState = GPIO_getInputPinValue(GPIO_PORT_P3, GPIO_PIN7);
          /* Debounce the button until next interrupt (50 milliseconds)
           * so wait another 50 milliseconds to see if the button is still low
           */
          if (currentFireButtonState == previousFireButtonState && currentFireButtonState == GPIO_INPUT_PIN_LOW)
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
                 Timer_A_outputPWMParam pwmBuzzerParam1 =
                 {
                  TIMER_A_CLOCKSOURCE_ACLK,
                  TIMER_A_CLOCKSOURCE_DIVIDER_1,
                  3200,
                  TIMER_A_CAPTURECOMPARE_REGISTER_2,
                  TIMER_A_OUTPUTMODE_RESET_SET,
                  3000
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
          for (i = 0; i < NUM_BULLETS; i++)
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
                   Graphics_fillRectangleOnDisplay(g_sContext.display, &bulletsRect, g_sContext.background);
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
                        for (j = 0; j < NUM_ALIENS; j++)
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
                                 Timer_A_outputPWMParam pwmBuzzerParam1 =
                                 {
                                  TIMER_A_CLOCKSOURCE_ACLK,
                                  TIMER_A_CLOCKSOURCE_DIVIDER_1,
                                  3200,
                                  TIMER_A_CAPTURECOMPARE_REGISTER_2,
                                  TIMER_A_OUTPUTMODE_RESET_SET,
                                  3000
                                 };
                                 Timer_A_outputPWM(TIMER_A2_BASE, &pwmBuzzerParam1);
                                 pwmExplosionCounter = 0;
                              /* Clear the aliens previous position */
                              alienRect.xMax = aliens[j][x] + 2;
                              alienRect.xMin = aliens[j][x] - 2;
                              alienRect.yMax = aliens[j][y] + 2;
                              alienRect.yMin = aliens[j][y] - 2;
                              Graphics_fillRectangleOnDisplay(g_sContext.display, &alienRect, g_sContext.background);
                                /* Kill the alien */
                                aliens[j][x] = 200;
                                aliens[j][y] = 200;

                                /* Clear the bullet */
                                /* Clear the bullets position */
                              bulletsRect.xMax = bullets[i][x] + 2;
                              bulletsRect.xMin = bullets[i][x] - 2;
                              bulletsRect.yMax = bullets[i][y] + 2;
                              bulletsRect.yMin = bullets[i][y] - 2;
                              Graphics_fillRectangleOnDisplay(g_sContext.display, &bulletsRect, g_sContext.background);

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
          break;
      default: break;
  }
}

#pragma vector=TIMER1_A1_VECTOR
__interrupt void TIMER1_A1_ISR(void)
{
    switch (__even_in_range(TA1IV, 4))
      {
        case 2: // CCR1 IFG
        {
            /* Draw the aliens */
            uint8_t i;
            for (i = 0; i < NUM_ALIENS; i++)
            {
                /* If the aliens aren't dead... */
                if (aliens[i][x] != 200 && aliens[i][y] != 200)
                {
                    /* Clear the aliens previous position */
                    alienRect.xMax = previousAliens[i][x] + 2;
                    alienRect.xMin = previousAliens[i][x] - 2;
                    alienRect.yMax = previousAliens[i][y] + 2;
                    alienRect.yMin = previousAliens[i][y] - 2;
                   Graphics_fillRectangleOnDisplay(g_sContext.display, &alienRect, g_sContext.background);

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
                       printf("[Alien] Movement should not be reached");
                   }
                   /* Draw the alien at its new position */
                   alienRect.xMax = aliens[i][x] + 2;
                   alienRect.xMin = aliens[i][x] - 2;
                   alienRect.yMax = aliens[i][y] + 2;
                   alienRect.yMin = aliens[i][y] - 2;
                  Graphics_fillRectangle(&g_sContext, &alienRect);
                }
                else
                {
                    /* Don't draw the alien because it is dead */
                }
            }
            /* Advance the explosions */
              for (i = 0; i < NUM_ALIENS; i++)
              {
                  /* If the explosion has been set to be shown */
                  if (explosions[i][s])
                  {
                      /* If the explosion is set to increase in direction */
                      if (explosions[i][d] == 1)
                      {
                          /* Don't need to clear previous circle because the explosion gets bigger */
                          Graphics_fillCircle(&g_sContext, explosions[i][x], explosions[i][y], explosions[i][r]);
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
                            Graphics_fillRectangleOnDisplay(g_sContext.display, &explosionRect, g_sContext.background);
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
                              Graphics_fillCircle(&g_sContext, explosions[i][x], explosions[i][y], explosions[i][r]);
                          }
                      }
                  }
              }
            break;
        }
        default: break;
      }
}
