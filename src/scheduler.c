#include "inc/scheduler.h"
#include <msp430.h>

uint32_t *initializeStack(void (*func)(void), uint32_t *stackLocation)
{
  uint32_t *lStackLocation;
  uint16_t *sStackLocation;

  sStackLocation = (uint16_t*) stackLocation;
  *sStackLocation = (uint16_t)func;
  sStackLocation--;
  *sStackLocation = (uint16_t) (((0xF0000 & (uint32_t) func) >> 4) | GIE);

  sStackLocation -= sizeof(uint32_t) / 2;
  lStackLocation = (uint32_t*) sStackLocation;

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

  return lStackLocation;
}

