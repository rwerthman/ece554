#include <msp430.h>
#include <driverlib.h>
#include <grlib.h>
#include "Crystalfontz128x128_ST7735.h"
#include <stdio.h>

void testFunction(void);
void initWatchdog(void);
void initClocks(void);
void initTimers(void);
void initJoystickADC(void);
uint32_t mapValToRange(uint32_t x, uint32_t input_min, uint32_t input_max, uint32_t output_min, uint32_t output_max);

#define MCLK_SMCLK_DESIRED_FREQUENCY_IN_KHZ 24000

/* Graphic library context */
Graphics_Context g_sContext;

/* ADC results buffer */
static uint32_t resultsBuffer[2];

/**
 * main.c
 */
int main(void)
{
	initWatchdog();
	initClocks();
	initJoystickADC();
	
	mapValToRange(5, 0, 4095, 0, 127);

	testFunction();

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
    Graphics_drawStringCentered(&g_sContext,
                                (int8_t *)"Joystick:",
                                AUTO_STRING_LENGTH,
                                64,
                                30,
                                OPAQUE_TEXT);

    // Globally enable interrupts
    __bis_SR_register(GIE);

    //Enable/Start first sampling and conversion cycle
    ADC12_A_startConversion(ADC12_A_BASE,
                ADC12_A_MEMORY_0,
                ADC12_A_REPEATED_SEQOFCHANNELS);

    while (1)
    {


    }

	return 0;
}

void testFunction(void)
{
    int test = 0;
}

void initWatchdog(void)
{
    // Stop the watchdog timer
    WDT_A_hold( WDT_A_BASE );
}

void initClocks(void)
{
    // Set the core voltage level to handle a 24 MHz clock rate
    PMM_setVCore( PMM_CORE_LEVEL_3 );

    // Set ACLK to use REFO as its oscillator source (32KHz)
    UCS_initClockSignal(UCS_ACLK,
                        UCS_REFOCLK_SELECT,
                        UCS_CLOCK_DIVIDER_1);

    // Set REFO as the oscillator reference clock for the FLL
    UCS_initClockSignal(UCS_FLLREF,
                        UCS_REFOCLK_SELECT,
                        UCS_CLOCK_DIVIDER_1);

    // Set MCLK and SMCLK to use the DCO/FLL as their oscillator source (16MHz)
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

    ADC12_A_init(ADC12_A_BASE,
                 ADC12_A_SAMPLEHOLDSOURCE_SC,
                 ADC12_A_CLOCKSOURCE_SMCLK,
                 ADC12_A_CLOCKDIVIDER_1);

    ADC12_A_enable(ADC12_A_BASE);

    ADC12_A_setupSamplingTimer(ADC12_A_BASE,
            ADC12_A_CYCLEHOLD_16_CYCLES,
            ADC12_A_CYCLEHOLD_4_CYCLES,
            ADC12_A_MULTIPLESAMPLESENABLE);

    ADC12_A_configureMemoryParam param0 =
    {
     ADC12_A_MEMORY_0,
     ADC12_A_INPUT_A5,
     ADC12_A_VREFPOS_AVCC,
     ADC12_A_VREFNEG_AVSS,
     ADC12_A_NOTENDOFSEQUENCE
    };

    ADC12_A_configureMemory(ADC12_A_BASE, &param0);

    ADC12_A_configureMemoryParam param1 =
       {
        ADC12_A_MEMORY_1,
        ADC12_A_INPUT_A3,
        ADC12_A_VREFPOS_AVCC,
        ADC12_A_VREFNEG_AVSS,
        ADC12_A_ENDOFSEQUENCE
       };

       ADC12_A_configureMemory(ADC12_A_BASE, &param1);

    //Enable memory buffer 1 interrupts
    // so interrupt fires when sequence is completed
    ADC12_A_clearInterrupt(ADC12_A_BASE,
        ADC12IFG1);
    ADC12_A_enableInterrupt(ADC12_A_BASE,
        ADC12IE1);
}

// https://stackoverflow.com/questions/5731863/mapping-a-numeric-range-onto-another
// https://github.com/arduino/Arduino/issues/2466
uint32_t mapValToRange(uint32_t x, uint32_t input_min, uint32_t input_max, uint32_t output_min, uint32_t output_max)
{
    uint32_t val = ((x - input_min)*(output_max - output_min + 1)/(input_max - input_min + 1)) + output_min;
    return val;
}

void initTimers(void)
{

}

#pragma vector=ADC12_VECTOR
__interrupt void ADC12_ISR (void)
{
    switch (__even_in_range(ADC12IV,34)){
        case  0: break;   //Vector  0:  No interrupt
        case  2: break;   //Vector  2:  ADC overflow
        case  4: break;   //Vector  4:  ADC timing overflow
        case  6: break;        //Vector  6:  ADC12IFG0
        case  8:
            resultsBuffer[0] = (uint32_t)ADC12_A_getResults(ADC12_A_BASE, ADC12_A_MEMORY_0);
            resultsBuffer[1] = (uint32_t)ADC12_A_getResults(ADC12_A_BASE, ADC12_A_MEMORY_1);

            char string[10];
            sprintf(string, "X: %5lu", resultsBuffer[0]);
            Graphics_drawStringCentered(&g_sContext,
                                           (int8_t *)string,
                                           8,
                                           64,
                                           50,
                                           OPAQUE_TEXT);
            sprintf(string, "Y: %5lu", resultsBuffer[1]);
            Graphics_drawStringCentered(&g_sContext,
                                       (int8_t *)string,
                                       8,
                                       64,
                                       70,
                                       OPAQUE_TEXT);
            Graphics_Rectangle rect;
//            rect.xMax = mapValToRange(resultsBuffer[0] + 1, 0UL, 4095UL, 0UL, 127UL);
//            rect.xMin = mapValToRange(resultsBuffer[0] - 1, 0UL, 4095UL, 0UL, 127UL);
//            rect.yMax = mapValToRange(resultsBuffer[1] + 1, 0UL, 4095UL, 0UL, 127UL);
//            rect.yMin = mapValToRange(resultsBuffer[1] - 1, 0UL, 4095UL, 0UL, 127UL);
            Graphics_fillRectangle(&g_sContext, &rect);
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
