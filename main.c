#include <msp430.h>
#include <driverlib.h>
#include <grlib.h>
#include "Crystalfontz128x128_ST7735.h"
#include <stdio.h>

void initWatchdog(void);
void initClocks(void);
void initTimers(void);
void initJoystickADC(void);
void initBullets(void);
uint32_t mapValToRange(uint32_t x, uint32_t input_min, uint32_t input_max, uint32_t output_min, uint32_t output_max);

#define MCLK_SMCLK_DESIRED_FREQUENCY_IN_KHZ 25000

/* Graphic library context */
Graphics_Context g_sContext;

/* ADC results buffer */
static uint32_t spacecraftPosition[2];

/* Fire button S2  value */
uint8_t currentFireButtonState;
uint8_t previousFireButtonState;

/* Spacecraft bullets */
enum
{
    x = 0,
    y = 1
};

#define NUM_BULLETS (uint8_t)10
uint16_t bullets[NUM_BULLETS][2];
uint8_t currentBullet = 0;

/**
 * main.c
 */
int main(void)
{
    uint8_t i;

	initWatchdog();
	initClocks();
	initTimers();
	initJoystickADC();
	initBullets();
	
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

    while (1)
    {
        /* Clear the screen */
        //Graphics_clearDisplay(&g_sContext);

        /* Redraw the spacecraft */
        Graphics_Rectangle rect;
        rect.xMax = spacecraftPosition[x] + 2;
        rect.xMin = spacecraftPosition[x] - 2;
        rect.yMax = spacecraftPosition[y] + 2;
        rect.yMin = spacecraftPosition[y] - 2;
        Graphics_fillRectangle(&g_sContext, &rect);

        /* Redraw the bullets */
        for (i = 0; i < NUM_BULLETS; i++)
        {
            /* If the bullets have been shot so they aren't the
             * initial values
             */
            if (bullets[i][x] != 200 && bullets[i][y] != 200)
            {
                rect.xMax = bullets[i][x] + 2;
                rect.xMin = bullets[i][x] - 2;
                rect.yMax = bullets[i][y] + 2;
                rect.yMin = bullets[i][y] - 2;
                Graphics_fillRectangle(&g_sContext, &rect);

                /* If the bullets have moved off the screen reset
                 * them
                 */
                if (bullets[i][y] == 0)
                {
                    bullets[i][x] = 200;
                    bullets[i][y] = 200;
                }
                else
                {
                    /* Show the bullets moving up to the top of the screen */
                    bullets[i][y]--;
                }
            }
        }
    }

	return 0;
}

void initWatchdog(void)
{
    // Stop the watchdog timer
    WDT_A_hold( WDT_A_BASE );
}

void initBullets(void)
{
    uint8_t i;
    for (i = 0; i < NUM_BULLETS; i++)
    {
        bullets[i][x] = 200;
        bullets[i][y] = 200;
    }
}

void initGPIO(void)
{
    /* Enable user button 2 on educational booster pack */
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P3, GPIO_PIN7);
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

    /* Configures Pin 6.3 (Joystick Y) and 6.5 (Joystick X) as ADC input */
    GPIO_setAsInputPin(GPIO_PORT_P6, GPIO_PIN3);
    GPIO_setAsInputPin(GPIO_PORT_P6, GPIO_PIN5);

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
}

#pragma vector=ADC12_VECTOR
__interrupt void ADC12_ISR(void)
{
    switch (__even_in_range(ADC12IV,34))
    {
        case  0: break;   //Vector  0:  No interrupt
        case  2: break;   //Vector  2:  ADC overflow
        case  4: break;   //Vector  4:  ADC timing overflow
        case  6: break;        //Vector  6:  ADC12IFG0
        case  8:
            spacecraftPosition[x] = mapValToRange((uint32_t)ADC12_A_getResults(ADC12_A_BASE, ADC12_A_MEMORY_0), 0UL, 255UL, 0UL, 127UL);
            spacecraftPosition[y] = mapValToRange((uint32_t)ADC12_A_getResults(ADC12_A_BASE, ADC12_A_MEMORY_1), 0UL, 255UL, 127UL, 0UL);
            break;   //Vector  8:  ADC12IFG1
        case 10: break;   //Vector 10:  ADC12IFG2
        case 12: break;   //Vector 12:  ADC12IFG3
        case 14: break;   //Vector 14:  ADC12IFG4
        case 16: break;   //Vector 16:  ADC12IFG5
        case 18: break;   //Vector 18:  ADC12IFG6
        case 20: break;   //Vector 20:  ADC12IFG7
        case 22: break;   //Vector 22:  ADC12IFG8
        case 24: break;   //Vector 24:  ADC12IFG9
        case 26: break;   //Vector 26:  ADC12IFG10
        case 28: break;   //Vector 28:  ADC12IFG11
        case 30: break;   //Vector 30:  ADC12IFG12
        case 32: break;   //Vector 32:  ADC12IFG13
        case 34: break;   //Vector 34:  ADC12IFG14
        default: break;
    }
}

#pragma vector=TIMER0_A1_VECTOR
__interrupt void TIMER0_A1_ISR(void)
{
  switch (__even_in_range(TA0IV, 12))
  {
      case 0: break; // None
      case 2: break; // CCR1 IFG
      case 4:
          /* Read shoot button input */
          currentFireButtonState = GPIO_getInputPinValue(GPIO_PORT_P3, GPIO_PIN7);
          /* Debounce the button until next interrupt (50 milliseconds) */
          if (currentFireButtonState == previousFireButtonState && currentFireButtonState == GPIO_INPUT_PIN_LOW)
          {
              /* Shoot a bullet */
              bullets[currentBullet][x] = spacecraftPosition[x];
              bullets[currentBullet][y] = spacecraftPosition[y];
              /* Go to the next bullet or back to 0 if there isn't
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
              /* Reset previous state to ready for next button press */
              previousFireButtonState = GPIO_INPUT_PIN_HIGH;
          }
          else
          {
              previousFireButtonState = currentFireButtonState;
          }
          break; // CCR2
      case 6: break; // CCR3
      case 8: break; // CCR4
      case 10: break; // CCR5
      case 12: break; // Overflow
      default: break;
  }
}
