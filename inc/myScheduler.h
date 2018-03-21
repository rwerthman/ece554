#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include <stdint.h>
#define NUM_TASKS (uint8_t)4

extern uint32_t *stack_pointer[NUM_TASKS]; // Stores the stack pointer for each task
extern int8_t currentRunningTask;
extern uint32_t *tempSP;

void addTaskToScheduler(void (*func)(void), uint32_t *stackLocation);
uint32_t *initializeStack(void (*func)(void), uint32_t *stackLocation);
void startScheduler(void);

#endif /* SCHEDULER_H_ */
