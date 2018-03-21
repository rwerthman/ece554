#include <inc/mySemaphore.h>
#include <stdio.h>

void addCallerToSem(semType *sem, semCaller *c);
void removeCallerFromSem(semType *sem, semCaller *c);

void wait(semType *sem, semCaller *c)
{
  __no_operation();
  __disable_interrupt();
  __no_operation();
  addCallerToSem(sem, c);
  /* Wait while the semaphore is busy AND the caller is not the first semaphore caller */
  for (;;)
  {
    __no_operation();
    __enable_interrupt();
    __no_operation();
    __disable_interrupt();
    __no_operation();
    /* If the semaphore is available... */
    if (sem->value == 1)
    {
      /* And this task is up first on the queue */
      if (sem->first->id == c->id)
      {
        /* Break out and take the semaphore */
        break;
      }
    }
  }

  if (sem->first->id != c->id)
  {
    /* This shouldn't be reached.
     * The first thing on the queue should match
     * the calling task that has the semaphore
     */
    while (1);
  }

  sem->value = 0;
  __no_operation();
  __enable_interrupt();
  __no_operation();
}

void signal(semType *sem, semCaller *c)
{
  __no_operation();
  __disable_interrupt();
  __no_operation();
  if (sem->first->id != c->id)
  {
    /* This shouldn't be reached
     * The caller that signals should be first caller on sem queue
     */
    while (1);
  }
  removeCallerFromSem(sem, c);
  /* Make the semaphore available */
  sem->value = 1;
  __no_operation();
  __enable_interrupt();
  __no_operation();
}

void addCallerToSem(semType *sem, semCaller *c)
{
  /* Point the last thing in the queue
   * to the new one.
   */
  if (sem->last != 0)
  {
    sem->last->next = c;
  }
  /* Set the last pointer to the new caller */
  sem->last = c;
  /* Set the last and first pointer to eachother
   * if there is only one thing in the queue.
   */
  if (sem->first == 0)
  {
    sem->first = sem->last;
  }
}

void removeCallerFromSem(semType *sem, semCaller *c)
{
  semCaller *prev;
  if (sem->first == 0)
  {
    /* This shouldn't be reached
     * this means the caller queue was empty
     */
    while (1);
  }
  prev = sem->first;
  /* Move to the next one in the queue */
  sem->first = sem->first->next;
  /* Reset sem caller next pointer */
  prev->next = 0;
  /* If first is null then reset last pointer */
  if (sem->first == 0)
  {
    sem->last = 0;
  }
}
