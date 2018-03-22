#include <driverlib.h>
#include "inc/myTimers.h"

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

    /* Debouce fire button timer */
    Timer_A_initCompareModeParam compareParamA1_2 =
    {
      TIMER_A_CAPTURECOMPARE_REGISTER_2,
      TIMER_A_CAPTURECOMPARE_INTERRUPT_DISABLE,
      TIMER_A_OUTPUTMODE_SET_RESET,
      2*320, // 20 milliseconds
    };
    Timer_A_initCompareMode(TIMER_A1_BASE, &compareParamA1_2);
    Timer_A_clearCaptureCompareInterrupt(TIMER_A1_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_2);
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

