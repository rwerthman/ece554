#include "inc/semaphore.h"

void wait(semType *sem, semCaller *c)
{
  __disable_interrupt();
  if (sem->currentSemCaller == 0)
  {
    sem->currentSemCaller = c;
  }
  else
  {
    /* Go to the next null pointer in the semaphore caller linked list */
    semCaller *temp, *curr;
    temp = sem->currentSemCaller;
     while (temp != 0)
     {
       curr = temp;
       temp = temp->nextSemCaller;
     }
     curr->nextSemCaller = c;
  }
  __enable_interrupt();
  while ((sem->value == 0) || (sem->currentSemCaller->id != c->id));
  sem->value = 0;
}

void signal(semType *sem, semCaller *c)
{
  __disable_interrupt();
  if (sem->currentSemCaller != 0)
  {
    sem->currentSemCaller = sem->currentSemCaller->nextSemCaller;
    c->nextSemCaller = 0;
  }
  else
  {
    //printf("[Sem Signal]  This should never be reached.\n");
  }
  sem->value = 1;
  __enable_interrupt();
}
