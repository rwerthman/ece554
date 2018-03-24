#include <driverlib.h>
#include <inc/myScheduler.h>

uint32_t *stack_pointer[NUM_TASKS]; // Stores the stack pointer for each task
int8_t currentRunningTask;
uint32_t *tempSP;

static uint8_t currentTaskStackPointer = 0;

void addTaskToScheduler(void (*func)(void), uint32_t *stackLocation)
{
  stack_pointer[currentTaskStackPointer] = initializeStack(func, stackLocation);
  currentTaskStackPointer++;
}

uint32_t *initializeStack(void (*func)(void), uint32_t *stackLocation)
{
  uint32_t *lStackLocation;
  uint16_t *sStackLocation;

  sStackLocation = (uint16_t*) stackLocation;
  /* Set the program counter (the first 16 bits of the stack) to the beginning of the task function */
  *sStackLocation = (uint16_t)func;
  sStackLocation--;
  /* Add the status register and bits 19 - 16 of the program counter to the next 16 bits of the stack
   * in the manner specified by the msp430f5529 datasheet
   */
  *sStackLocation = (uint16_t) (((0xF0000 & (uint32_t) func) >> 4) | GIE);


  /* Go to the next 32 bits */
  sStackLocation -= sizeof(uint32_t) / 2;
  lStackLocation = (uint32_t*) sStackLocation;

  /* Make room for all of the general purpose registers R4 - R15 */
  *lStackLocation = (uint32_t) 0xFFFF;
  lStackLocation--;
  *lStackLocation = (uint32_t) 0xFFFF;
  lStackLocation--;
  *lStackLocation = (uint32_t) 0xFFFF;
  lStackLocation--;
  *lStackLocation = (uint32_t) 0xFFFF;
  lStackLocation--;
  *lStackLocation = (uint32_t) 0xFFFF;
  lStackLocation--;
  *lStackLocation = (uint32_t) 0xFFFF;
  lStackLocation--;
  *lStackLocation = (uint32_t) 0xFFFF;
  lStackLocation--;
  *lStackLocation = (uint32_t) 0xFFFF;
  lStackLocation--;
  *lStackLocation = (uint32_t) 0xFFFF;
  lStackLocation--;
  *lStackLocation = (uint32_t) 0xFFFF;
  lStackLocation--;
  *lStackLocation = (uint32_t) 0xFFFF;
  lStackLocation--;
  *lStackLocation = (uint32_t) 0xFFFF;

  /* Return a pointer to the end of the buttom (end) of the stack */
  return lStackLocation;
}

void startScheduler(void)
{
  /* Set this to tell the scheduler how to handle the first context switch */
  currentRunningTask = NUM_TASKS;

  // Globally enable interrupts
   __bis_SR_register(GIE);

   // Enable/Start first sampling and conversion cycle of the Joystick ADC
   ADC12_A_startConversion(ADC12_A_BASE, ADC12_A_MEMORY_0,
                           ADC12_A_REPEATED_SEQOFCHANNELS);

   // Start the timer to trigger the Joystick ADC and scheduler
   Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);

   // Start the timer to trigger the Aliens and fire button
   Timer_A_startCounter(TIMER_A1_BASE, TIMER_A_UP_MODE);

   // Start the timer to backlight, and shooting and explosion sounds
   Timer_A_startCounter(TIMER_A2_BASE, TIMER_A_UP_MODE);
  while (1)
  {

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

