#ifndef SEMAPHORE_H_
#define SEMAPHORE_H_
#include <stdint.h>

typedef struct semCaller
{
  uint8_t id;
  struct semCaller *nextSemCaller;
} semCaller;

typedef struct semType
{
  uint8_t value;
  semCaller *currentSemCaller;
} semType;

void wait(semType *sem, semCaller *c);
void signal(semType *sem, semCaller *c);

#endif /* SEMAPHORE_H_ */
