#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include <stdint.h>

uint32_t *initializeStack(void (*func)(void), uint32_t *stackLocation);

#endif /* SCHEDULER_H_ */
