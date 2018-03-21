#ifndef SEMAPHORE_H_
#define SEMAPHORE_H_
#include <stdint.h>

typedef struct semCaller
{
  uint8_t id;
  struct semCaller *next;
} semCaller;

typedef struct semType
{
  int8_t value;
  semCaller *first;
  semCaller *last;
} semType;

void wait(semType *sem, semCaller *c);
void signal(semType *sem, semCaller *c);

#endif /* SEMAPHORE_H_ */
