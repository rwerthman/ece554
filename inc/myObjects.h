#ifndef INC_MYOBJECTS_H_
#define INC_MYOBJECTS_H_

#include <stdint.h>

#define NUM_ALIENS (uint8_t)3
#define NUM_EXPLOSIONS (uint8_t)3
#define NUM_BULLETS (uint8_t)10

/* Spacecraft globals */
extern uint32_t spacecraftPosition[2];
extern uint32_t previousSpacecraftPosition[2];

/* Bullet globals */
extern uint16_t bullets[NUM_BULLETS][2];
extern uint16_t previousBullets[NUM_BULLETS][2];
extern uint8_t currentBullet;

/* Fire button S2  value */
extern uint8_t currentFireButtonState;
extern uint8_t previousFireButtonState;

/* Aliens globals */
extern uint16_t aliens[NUM_ALIENS][2];
extern uint16_t previousAliens[NUM_ALIENS][2];

/* Explosion globals */
enum
{
  x = 0,
  y = 1,
  s = 2, // Enable or not
  r = 3, // Radius
  d = 4  // Direction
};
extern uint16_t explosions[NUM_ALIENS][5];

/* Sound globals */
extern uint8_t pwmFireCounter;
extern uint8_t pwmExplosionCounter;

void initObjects(void);


#endif /* INC_MYOBJECTS_H_ */
