#include "driverlib.h"

#define MCLK_SMCLK_DESIRED_FREQUENCY_IN_KHZ 25000

void initClocks(void);
void initTimers(void);
uint32_t *initializeStack(void (*func)(void), uint32_t *stackLocation);
void task1(void);
void task2(void);

#define NUM_TASKS (uint8_t)2
uint32_t *stack_pointer[NUM_TASKS]; // Stores the stack pointer for each task

#define STACK_SIZE 64
uint8_t currentRunningTask;
uint32_t task1Stack[STACK_SIZE];
uint32_t task2Stack[STACK_SIZE];

uint32_t *tempSP;

int main(void) {
    WDT_A_hold(WDT_A_BASE);
    initClocks();
    initTimers();

    /* Pass in the top of the stack which is the highest memory location
     * because the stack grows downward.
     */
    stack_pointer[0] = initializeStack(task1, &task1Stack[STACK_SIZE-1]);
    stack_pointer[1] = initializeStack(task2, &task2Stack[STACK_SIZE-1]);

    /* Set current running task to NUM_TASKS so we don't push for the first task */
    currentRunningTask = NUM_TASKS;

    __enable_interrupt();
    Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);
    /* Wait for the first task to start */
    while (1)
    {

    }
}

uint32_t *initializeStack(void (*func)(void), uint32_t *stackLocation)
{
    uint32_t *lStackLocation;
    uint16_t *sStackLocation, sTemp;

    sStackLocation = (uint16_t*)stackLocation;
    sTemp = (uint16_t)func;
    *sStackLocation = sTemp;
    sStackLocation--;
    *sStackLocation = (uint16_t)(((0xF0000 & (uint32_t)func) >> 4) | GIE);

    sStackLocation -= sizeof(uint32_t)/2;
    lStackLocation = (uint32_t*)sStackLocation;

    *lStackLocation = (uint32_t)0xFFFF;
    lStackLocation--;
    *lStackLocation = (uint32_t)0xFFFF;
    lStackLocation--;
    *lStackLocation = (uint32_t)0xFFFF;
    lStackLocation--;
    *lStackLocation = (uint32_t)0xFFFF;
    lStackLocation--;
    *lStackLocation = (uint32_t)0xFFFF;
    lStackLocation--;
    *lStackLocation = (uint32_t)0xFFFF;
    lStackLocation--;
    *lStackLocation = (uint32_t)0xFFFF;
    lStackLocation--;
    *lStackLocation = (uint32_t)0xFFFF;
    lStackLocation--;
    *lStackLocation = (uint32_t)0xFFFF;
    lStackLocation--;
    *lStackLocation = (uint32_t)0xFFFF;
    lStackLocation--;
    *lStackLocation = (uint32_t)0xFFFF;
    lStackLocation--;
    *lStackLocation = (uint32_t)0xFFFF;

    return lStackLocation;
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
     TIMER_A_CAPTURECOMPARE_INTERRUPT_ENABLE,
     TIMER_A_OUTPUTMODE_SET_RESET,
     3200, // Counted up to in 100 milliseconds (.1 second) with a 32 KHz clock
    };
    Timer_A_initCompareMode(TIMER_A0_BASE, &compareParam1);
    Timer_A_clearTimerInterrupt(TIMER_A0_BASE);

    Timer_A_clearCaptureCompareInterrupt(TIMER_A0_BASE,
                                             TIMER_A_CAPTURECOMPARE_REGISTER_0 +
                                             TIMER_A_CAPTURECOMPARE_REGISTER_1);
}

void task1(void)
{
    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0);
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0);
    while (1)
    {
        __delay_cycles(10000);
        GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN0);
        __delay_cycles(10000);
        GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0);
    }
}

void task2(void)
{
        GPIO_setAsOutputPin(GPIO_PORT_P4, GPIO_PIN7);
        GPIO_setOutputLowOnPin(GPIO_PORT_P4, GPIO_PIN7);
        while (1)
        {
            __delay_cycles(10000);
            GPIO_setOutputHighOnPin(GPIO_PORT_P4, GPIO_PIN7);
            __delay_cycles(10000);
            GPIO_setOutputLowOnPin(GPIO_PORT_P4, GPIO_PIN7);
        }
}

#pragma vector=TIMER0_A1_VECTOR
__interrupt void TIMER0_A1_ISR(void)
{
  switch (__even_in_range(TA0IV, 4))
  {
      case 2: // CCR1 IFG
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
            if (currentRunningTask < 1)
            {
                currentRunningTask++;
            }
            else
            {
                currentRunningTask = 0;
            }
            tempSP = stack_pointer[currentRunningTask];
            asm(" mov.a tempSP, sp");
          /* Pop the 16 bits of the general purpose registers R4 - R15 off of the stack restoring the context of the current task */
            __asm(" popm.a #12, r15");
          __asm(" reti");
          break;
      default:
          break;
  }
  /* Done by MSP430
   * The Status register and Program counter of popped off of the stack with the reti (return from interrupt routine) instruction
   * the GIE bit is set to whatever the status register is so more than likely enabled
   * */
}
