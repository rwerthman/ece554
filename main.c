#include <msp430.h>
#include <driverlib.h>
#include <grlib.h>
#include "Crystalfontz128x128_ST7735.h"

void initWatchdog(void);
void initClocks(void);
void initTimers(void);

#define MCLK_SMCLK_DESIRED_FREQUENCY_IN_KHZ 24000

uint32_t myACLK  = 0;
uint32_t mySMCLK = 0;
uint32_t myMCLK  = 0;

/* Graphic library context */
Graphics_Context g_sContext;

/**
 * main.c
 */
int main(void)
{
	initWatchdog();
	initClocks();
	
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

	// Verify that the modified clock settings are as expected
    myACLK  = UCS_getACLK();
    mySMCLK = UCS_getSMCLK();
    myMCLK  = UCS_getMCLK();

	return 0;
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

void initTimers(void)
{

}
