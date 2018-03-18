#include <inc/mySemaphore.h>

void wait(semType *sem, semCaller *c)
{
  __disable_interrupt();
  if (sem->currentSemCaller == 0)
  {
    /* First caller owns the semaphore */
    sem->currentSemCaller = c;
  }
  else
  {
    /* Go to the next null pointer in the semaphore caller linked list
     * and add the caller to it so it will eventually hold the semaphore
     */
    semCaller *next, *curr;
    next = sem->currentSemCaller;
    while (next != 0)
    {
      if (next->id == c->id)
      {
        /* If the caller is already holding the semaphore
         * don't add it to the list.  This should not be reached.
         */
        while (1);
      }
      curr = next;
      next = next->nextSemCaller;
    }
    curr->nextSemCaller = c;
  }
  __enable_interrupt();
  /* Wait while the semaphore is busy AND the caller is not the current semaphore caller */
  while ((sem->value == 0) && (sem->currentSemCaller->id != c->id));
  sem->value = 0;
}

void signal(semType *sem, semCaller *c)
{
  __disable_interrupt();
  if (sem->currentSemCaller != 0)
  {
    /* Remove this caller from the semaphore caller list */
    if (sem->currentSemCaller->id == c->id)
    {
      sem->currentSemCaller = sem->currentSemCaller->nextSemCaller;
      /* Point the caller to nothing */
      c->nextSemCaller = 0;
    }
    else
    {
      // Shouldn't be reached
      while (1);
    }
  }
  else
  {
    // Shouldn't be reached
    while (1);
  }
  /* Make the semaphore available */
  sem->value = 1;
  __enable_interrupt();
}
