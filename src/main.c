/**
 * TODO:
 * 1. Buffering for ADC input so all of the input will be drawn?
 * 2. Semaphores or mailboxes for shared resources between tasks
 * 3. Pull out scheduler into separate header and c file --- DONE ---
 * 4. Slow down the drawing of the explosions
 * 5. Fix problem with semaphores signaling when they shouldn't from different tasks
 *
 * High pitch = small period
 * Low pitch = high period
 */

#include <msp430.h>
#include <driverlib.h>
#include <grlib.h>
#include "inc/myScheduler.h"
#include "inc/mySemaphore.h"
#include "inc/Crystalfontz128x128_ST7735.h"
#include "inc/myTimers.h"
#include "inc/myClocks.h"
#include "inc/myGPIO.h"
#include "inc/myObjects.h"
#include "inc/myJoystickADC.h"

#define STACK_SIZE 64 // 64*32 = 2048 btyes

/* Utility functions */
uint32_t mapValToRange(uint32_t x, uint32_t input_min, uint32_t input_max,
                       uint32_t output_min, uint32_t output_max);

/* Task globals */
/* Tasks to be run and task stacks */
void drawSpacecraft(void);
uint32_t drawSpacecraftStack[STACK_SIZE];

void drawAliens(void);
uint32_t drawAliensStack[STACK_SIZE];

void drawBombs(void);
uint32_t drawBombsStack[STACK_SIZE];

void drawExplosions(void);
uint32_t drawExplosionsStack[STACK_SIZE];

void drawBullets(void);
uint32_t drawBulletsStack[STACK_SIZE];

uint8_t bombTimerCounter = 0;

/* Semaphores for the graphic's context */
semCaller drawSpacecraftGraphicsCaller = {4,0};
semCaller drawAliensGraphicsCaller ={1,0};
semCaller drawExplosionsGraphicsCaller ={2,0};
semCaller drawBulletsGraphicsCaller = {3,0};
semCaller drawBombsGraphicsCaller = {5,0};
semType graphicsSemaphore ={1,0,0};

/* Graphics objects */
Graphics_Context g_sContext;
Graphics_Rectangle spacecraftRect;
Graphics_Rectangle bulletsRect;
Graphics_Rectangle explosionRect;
Graphics_Rectangle alienRect;
Graphics_Rectangle bombRect;


Timer_A_outputPWMParam shootingPWM =
{
  TIMER_A_CLOCKSOURCE_SMCLK,
  TIMER_A_CLOCKSOURCE_DIVIDER_1,
  60000,
  TIMER_A_CAPTURECOMPARE_REGISTER_2,
  TIMER_A_OUTPUTMODE_RESET_SET,
  60000/2 // 50% duty cycle
};

Timer_A_outputPWMParam explosionPWM = {
  TIMER_A_CLOCKSOURCE_SMCLK,
  TIMER_A_CLOCKSOURCE_DIVIDER_1,
  6400,
  TIMER_A_CAPTURECOMPARE_REGISTER_2,
  TIMER_A_OUTPUTMODE_RESET_SET,
  6400/2 // 50% duty cycle
};


/**
 * main.c
 */
void main(void)
{

  // Stop the watchdog timer
  WDT_A_hold(WDT_A_BASE);

  initClocks();
  initTimers();
  initGPIO();
  initJoystickADC();
  initObjects();

  /* Initializes display */
  Crystalfontz128x128_Init();

  /* Set default screen orientation */
  Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP);

  /* Initializes graphics context */
  Graphics_initContext(&g_sContext, &g_sCrystalfontz128x128);
  Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_GREEN);
  Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_RED);
  GrContextFontSet(&g_sContext, &g_sFontFixed6x8);
  Graphics_clearDisplay(&g_sContext);

  addTaskToScheduler(drawSpacecraft, &drawSpacecraftStack[STACK_SIZE - 1]);
  addTaskToScheduler(drawAliens, &drawAliensStack[STACK_SIZE - 1]);
  addTaskToScheduler(drawBombs, &drawBombsStack[STACK_SIZE - 1]);
  addTaskToScheduler(drawBullets, &drawBulletsStack[STACK_SIZE - 1]);
  addTaskToScheduler(drawExplosions, &drawExplosionsStack[STACK_SIZE - 1]);

  startScheduler();

}

// https://stackoverflow.com/questions/5731863/mapping-a-numeric-range-onto-another
// https://github.com/arduino/Arduino/issues/2466
uint32_t mapValToRange(uint32_t x, uint32_t input_min, uint32_t input_max,
                       uint32_t output_min, uint32_t output_max)
{
  uint32_t val = (uint32_t) (((x - input_min) * (output_max - output_min + 1)
      / (input_max - input_min + 1)) + output_min);
  return val;
}

void drawBullets(void)
{
  while(1)
  {
    if (Timer_A_getCaptureCompareInterruptStatus(TIMER_A1_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_1, TIMER_A_CAPTURECOMPARE_INTERRUPT_FLAG))
    {
      Timer_A_clearCaptureCompareInterrupt(TIMER_A1_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_1);
      /* Read the "shoot" button input */
           currentFireButtonState = GPIO_getInputPinValue(GPIO_PORT_P3, GPIO_PIN7);
           /* Debounce the button until next interrupt (50 milliseconds)
            * so wait another 50 milliseconds to see if the button is still low
            */
           if (currentFireButtonState == previousFireButtonState &&
               currentFireButtonState == GPIO_INPUT_PIN_LOW)
           {
             /* If the button is low shoot a bullet */
             /* First store the bullets starting position */
             previousBullets[currentBullet][x] = bullets[currentBullet][x];
             previousBullets[currentBullet][y] = bullets[currentBullet][y];
             /* Set the bullet to the position of the spacecraft when
              * it was fired.
              */
             bullets[currentBullet][x] = spacecraftPosition[x];
             /* Subtract from the spacecraft position so the bullet looks like it is ahead of the
              * spacecraft when we shoot it and also we don't
              * clear the spacecraft when we clear the bullet
              */
             bullets[currentBullet][y] = spacecraftPosition[y] - 5;
             /* Advance to the next bullet or back to 0 if there isn't
              * another bullet
              */
             if (currentBullet < NUM_BULLETS)
             {
               currentBullet++;
             }
             else
             {
               currentBullet = 0;
             }
             /* Reset previous button state to be ready for the next button press */
             previousFireButtonState = GPIO_INPUT_PIN_HIGH;
             /* Generate PWM for the buzzer when shooting */
             Timer_A_outputPWM(TIMER_A2_BASE, &shootingPWM);
             pwmFireCounter = 0;
           }
           else
           {
             previousFireButtonState = currentFireButtonState;
             /* Let the pwm signal for the shot run for at least 3 context switches */
             if (pwmFireCounter < 3)
             {
               pwmFireCounter += 1;
             }
             else if (pwmFireCounter == 3)
             {
               Timer_A_stop(TIMER_A2_BASE);
               pwmFireCounter = 4;
             }

             /* If we aren't shooting and we can make the explosion noise */
             if (pwmExplosionCounter < 3 && pwmFireCounter == 4)
             {
               pwmExplosionCounter += 1;
             }
             /* If we aren't shooting and we are done with the explosion noises
              * turn of the PWM for the explosions
              * */
             else if (pwmFireCounter == 4 && pwmExplosionCounter == 3)
             {
               Timer_A_stop(TIMER_A2_BASE);
               pwmExplosionCounter = 4;
             }
           }
      /* Advance all of the bullets by clearing their previous positions
       * and drawing them at their new positions
       */
      uint8_t i;
      for (i = 0; i < NUM_BULLETS ; i++)
      {
        /* If the bullets have been shot which means they aren't the
         * initial values they were set to draw them on the screen
         */
        if (bullets[i][x] != 200 && bullets[i][y] != 200)
        {
          /* Clear the bullets previous position */
          bulletsRect.xMax = previousBullets[i][x] + 2;
          bulletsRect.xMin = previousBullets[i][x] - 2;
          bulletsRect.yMax = previousBullets[i][y] + 2;
          bulletsRect.yMin = previousBullets[i][y] - 2;
          wait(&graphicsSemaphore, &drawBulletsGraphicsCaller);
          Graphics_fillRectangleOnDisplay(g_sContext.display, &bulletsRect,
                                          g_sContext.background);
          signal(&graphicsSemaphore, &drawBulletsGraphicsCaller);
          /* If the bullet has moved off the screen reset
           * it.
           */
          if (bullets[i][y] <= 0)
          {
            bullets[i][x] = 200;
            bullets[i][y] = 200;

          }
          else
          {
            /* Store the bullets position before moving to the next one */
            previousBullets[i][x] = bullets[i][x];
            previousBullets[i][y] = bullets[i][y];
            /* Show the bullets moving up to the top of the screen */
            bullets[i][y]--;

            bulletsRect.xMax = bullets[i][x] + 2;
            bulletsRect.xMin = bullets[i][x] - 2;
            bulletsRect.yMax = bullets[i][y] + 2;
            bulletsRect.yMin = bullets[i][y] - 2;
            wait(&graphicsSemaphore, &drawBulletsGraphicsCaller);
            Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
            Graphics_fillRectangle(&g_sContext, &bulletsRect);
            Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_GREEN);
            signal(&graphicsSemaphore, &drawBulletsGraphicsCaller);
            /* Check if any of the aliens have been hit
             * by the bullet
             */
            uint8_t j;
            for (j = 0; j < NUM_ALIENS ; j++)
            {
              alienRect.xMax = aliens[j][x] + 2;
              alienRect.xMin = aliens[j][x] - 2;
              alienRect.yMax = aliens[j][y] + 2;
              alienRect.yMin = aliens[j][y] - 2;

              /* If a bullet and alien intersect... */
              if (Graphics_isRectangleOverlap(&bulletsRect, &alienRect))
              {
                /* Draw an explosion at the alien and bullet  */
                explosions[j][x] = aliens[j][x];
                explosions[j][y] = aliens[j][y];
                explosions[j][r] = 5;
                explosions[j][s] = 1;
                explosions[j][d] = 1;
                /* Generate PWM for the buzzer to make a sound of an explosion */
                Timer_A_outputPWM(TIMER_A2_BASE, &explosionPWM);
                pwmExplosionCounter = 0;
                /* Clear the aliens previous position */
                alienRect.xMax = aliens[j][x] + 2;
                alienRect.xMin = aliens[j][x] - 2;
                alienRect.yMax = aliens[j][y] + 2;
                alienRect.yMin = aliens[j][y] - 2;
                wait(&graphicsSemaphore, &drawBulletsGraphicsCaller);
                Graphics_fillRectangleOnDisplay(g_sContext.display, &alienRect,
                                                g_sContext.background);
                /* Kill the alien */
                aliens[j][x] = 200;
                aliens[j][y] = 200;

                /* Clear the bullet */
                /* Clear the bullets position */
                bulletsRect.xMax = bullets[i][x] + 2;
                bulletsRect.xMin = bullets[i][x] - 2;
                bulletsRect.yMax = bullets[i][y] + 2;
                bulletsRect.yMin = bullets[i][y] - 2;
                Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
                Graphics_fillRectangleOnDisplay(g_sContext.display, &bulletsRect,
                                                g_sContext.background);
                Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_GREEN);
                signal(&graphicsSemaphore, &drawBulletsGraphicsCaller);

                /* Set the bullet to its starting values
                 * so it is not redrawn again
                 */
                bullets[i][x] = 200;
                bullets[i][y] = 200;
                previousBullets[i][x] = bullets[i][x];
                previousBullets[i][y] = bullets[i][y];
              }
            }
          }
        }
      }
    }
  }
}

void drawExplosions(void)
{
  uint8_t i;
  while (1)
  {
    if (Timer_A_getCaptureCompareInterruptStatus(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_3, TIMER_A_CAPTURECOMPARE_INTERRUPT_FLAG))
    {
      Timer_A_clearCaptureCompareInterrupt(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_3);
      /* Advance the explosions */
      for (i = 0; i < NUM_EXPLOSIONS ; i++)
      {
        /* If the explosion has been set to be shown */
        if (explosions[i][s])
        {
          /* If the explosion is set to increase in direction */
          if (explosions[i][d] == 1)
          {
            /* Don't need to clear previous circle because the explosion gets bigger */
            wait(&graphicsSemaphore, &drawExplosionsGraphicsCaller);
            Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_PURPLE);
            Graphics_fillCircle(&g_sContext, explosions[i][x], explosions[i][y],
                                explosions[i][r]);
            Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_GREEN);
            signal(&graphicsSemaphore, &drawExplosionsGraphicsCaller);
            explosions[i][r] += 5;
            /* Max explosion radius should be 10 but we have to go to 15 to draw a circle with radius 10 */
            if (explosions[i][r] >= 15)
            {
              explosions[i][d] = 0;
              explosions[i][r] = 10;
            }
          }
          else
          {
            /* Clear previous explosion circle because circle gets smaller */
            explosionRect.xMax = explosions[i][x] + explosions[i][r];
            explosionRect.xMin = explosions[i][x] - explosions[i][r];
            explosionRect.yMax = explosions[i][y] + explosions[i][r];
            explosionRect.yMin = explosions[i][y] - explosions[i][r];
            wait(&graphicsSemaphore, &drawExplosionsGraphicsCaller);
            Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_PURPLE);
            Graphics_fillRectangleOnDisplay(g_sContext.display, &explosionRect,
                                            g_sContext.background);
            Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_GREEN);
            signal(&graphicsSemaphore, &drawExplosionsGraphicsCaller);
            /* Decrease the explosion size */
            explosions[i][r] -= 5;
            /* Minimum explosion size is 0.  Stop the explosion
             * when it gets to that size
             */
            if (explosions[i][r] <= 0)
            {
              /* Don't draw circle with radius 0 */
              explosions[i][s] = 0;
            }
            else
            {
              /* Otherwise keep drawing smaller explosions */
              wait(&graphicsSemaphore, &drawExplosionsGraphicsCaller);
              Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_PURPLE);
              Graphics_fillCircle(&g_sContext, explosions[i][x], explosions[i][y],
                                  explosions[i][r]);
              Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_GREEN);
              signal(&graphicsSemaphore, &drawExplosionsGraphicsCaller);
            }
          }
        }
      }
    }
  }
}

void drawAliens(void)
{
  /* Draw the aliens */
  uint8_t i;
  while (1)
  {
    if (Timer_A_getCaptureCompareInterruptStatus(TIMER_A1_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0, TIMER_A_CAPTURECOMPARE_INTERRUPT_FLAG))
    {
      Timer_A_clearCaptureCompareInterrupt(TIMER_A1_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0);
      for (i = 0; i < NUM_ALIENS ; i++)
      {
        /* If the aliens aren't dead... */
        if (aliens[i][x] != 200 && aliens[i][y] != 200)
        {
          wait(&graphicsSemaphore, &drawAliensGraphicsCaller);
          /* Clear the aliens previous position */
          alienRect.xMax = previousAliens[i][x] + 2;
          alienRect.xMin = previousAliens[i][x] - 2;
          alienRect.yMax = previousAliens[i][y] + 2;
          alienRect.yMin = previousAliens[i][y] - 2;
          Graphics_fillRectangleOnDisplay(g_sContext.display, &alienRect,
                                          g_sContext.background);
          /* Keep the aliens on the screen */
          /* Alien is moving to the right almost off the screen */
          if (aliens[i][x] > 125 && previousAliens[i][x] < aliens[i][x])
          {
            previousAliens[i][x] = aliens[i][x];
            aliens[i][x]--;
          }
          /* Alien is moving to the left almost off the screen */
          else if (aliens[i][x] < 2 && previousAliens[i][x] > aliens[i][x])
          {
            previousAliens[i][x] = aliens[i][x];
            aliens[i][x]++;
          }
          /* Alien is just moving to the right */
          else if (aliens[i][x] > previousAliens[i][x])
          {
            /* Keep alien moving to the right */
            previousAliens[i][x] = aliens[i][x];
            aliens[i][x]++;
          }
          /* Alien is just moving to the left */
          else if (aliens[i][x] < previousAliens[i][x])
          {
            /* Keep alien moving to the left */
            previousAliens[i][x] = aliens[i][x];
            aliens[i][x]--;
          }
          /* Alien hasn't started moving */
          else if (aliens[i][x] == previousAliens[i][x])
          {
            /* Choose a direction based on i */
            if (i % 2)
            {
              // To the right
              previousAliens[i][x] = aliens[i][x];
              aliens[i][x]++;
            }
            else
            {
              // To the left
              previousAliens[i][x] = aliens[i][x];
              aliens[i][x]--;
            }
          }
          else
          {
            /* This shouldn't be reached.
             * There shouldn't be any other way for the alien
             * to move.
             */
            while (1);
          }
          /* Draw the alien at its new position */
          alienRect.xMax = aliens[i][x] + 2;
          alienRect.xMin = aliens[i][x] - 2;
          alienRect.yMax = aliens[i][y] + 2;
          alienRect.yMin = aliens[i][y] - 2;
          Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_YELLOW);
          Graphics_fillRectangle(&g_sContext, &alienRect);
          Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_GREEN);
          signal(&graphicsSemaphore, &drawAliensGraphicsCaller);
        }
        else
        {
          /* Don't draw the alien because it is dead */
        }
      }
    }
  }
}

void drawBombs(void)
{
  uint8_t i;
  while (1)
   {
     /* If we have counted up to 1 second draw a bomb
      * What we want to do is initialize a bomb every second
      */
     if (bombTimerCounter > 8)
     {
       bombTimerCounter = 0;
       if (bombs[currentBomb][x] == 200 && bombs[currentBomb][y] == 200)
       {
         /* If we haven't shot the bombs, shoot the bombs from the aliens position */
         bombs[currentBomb][x] = aliens[currentBomb % 3][x];
         bombs[currentBomb][y] = aliens[currentBomb % 3][y];
       }

       /* Go to the next bomb */
       if (currentBomb < (NUM_BOMBS - 1))
       {
         currentBomb++;
       }
       else
       {
         currentBomb = 0;
       }
     }
     else if (Timer_A_getCaptureCompareInterruptStatus(TIMER_A1_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_2, TIMER_A_CAPTURECOMPARE_INTERRUPT_FLAG))
     {
       Timer_A_clearCaptureCompareInterrupt(TIMER_A1_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_2);
       bombTimerCounter++;
       for (i = 0; i < NUM_BOMBS; i++)
       {
         /* Draw the bombs if they aren't equal to their default values */
         if (bombs[i][x] != 200 && bombs[i][y] != 200)
         {
           wait(&graphicsSemaphore, &drawBombsGraphicsCaller);
           /* Store the bombs position */
           previousbombs[i][x] = bombs[i][x];
           previousbombs[i][y] = bombs[i][y];
           /* Erase the bombs position */
           bombRect.xMax = bombs[i][x] + 2;
           bombRect.xMin = bombs[i][x] - 2;
           bombRect.yMax = bombs[i][y] + 2;
           bombRect.yMin = bombs[i][y] - 2;
           Graphics_fillRectangleOnDisplay(g_sContext.display, &bombRect,
                                           g_sContext.background);
           /* Advance the bomb down the screen */
           bombs[i][y] += 3;
           /* Draw the bomb at the new position */
           bombRect.xMax = bombs[i][x] + 2;
           bombRect.xMin = bombs[i][x] - 2;
           bombRect.yMax = bombs[i][y] + 2;
           bombRect.yMin = bombs[i][y] - 2;
           Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_BROWN);
           Graphics_fillRectangle(&g_sContext, &bombRect);
           Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_GREEN);
           signal(&graphicsSemaphore, &drawBombsGraphicsCaller);

           /* Check if bombs are off screen */
           if (bombs[i][y] > 127)
           {
             /* Erase the bombs position */
            bombRect.xMax = bombs[i][x] + 2;
            bombRect.xMin = bombs[i][x] - 2;
            bombRect.yMax = bombs[i][y] + 2;
            bombRect.yMin = bombs[i][y] - 2;
            wait(&graphicsSemaphore, &drawBombsGraphicsCaller);
            Graphics_fillRectangleOnDisplay(g_sContext.display, &bombRect,
                                            g_sContext.background);
            signal(&graphicsSemaphore, &drawBombsGraphicsCaller);
             /* Reset the bombs */
             bombs[i][x] = 200;
             bombs[i][y] = 200;
           }
         }
          /* Check if the bombs and the spaceship collide */
          bombRect.xMax = bombs[i][x] + 2;
          bombRect.xMin = bombs[i][x] - 2;
          bombRect.yMax = bombs[i][y] + 2;
          bombRect.yMin = bombs[i][y] - 2;

          spacecraftRect.xMax = spacecraftPosition[x] + 2;
          spacecraftRect.xMin = spacecraftPosition[x] - 2;
          spacecraftRect.yMax = spacecraftPosition[y] + 2;
          spacecraftRect.yMin = spacecraftPosition[y] - 2;
          if (Graphics_isRectangleOverlap(&bombRect, &spacecraftRect))
          {
            /* Draw an explosion at the spacecraft and bomb  */
            explosions[3][x] = spacecraftPosition[x];
            explosions[3][y] = spacecraftPosition[y];
            explosions[3][r] = 5;
            explosions[3][s] = 1;
            explosions[3][d] = 1;
            /* Generate PWM for the buzzer to make a sound of an explosion */
            Timer_A_outputPWM(TIMER_A2_BASE, &explosionPWM);
            pwmExplosionCounter = 0;
            /* Clear the spacecrafts previous position */
            spacecraftRect.xMax = spacecraftPosition[x] + 2;
            spacecraftRect.xMin = spacecraftPosition[x] - 2;
            spacecraftRect.yMax = spacecraftPosition[y] + 2;
            spacecraftRect.yMin = spacecraftPosition[y] - 2;
            wait(&graphicsSemaphore, &drawBombsGraphicsCaller);
            Graphics_fillRectangleOnDisplay(g_sContext.display, &spacecraftRect,
                                            g_sContext.background);

            /* Stop drawing the spacecraft */
            spacecraftIsDestroyed = 1;
            spacecraftPosition[x] = 300;
            spacecraftPosition[y] = 300;
            previousSpacecraftPosition[x] = spacecraftPosition[x];
            previousSpacecraftPosition[y] = spacecraftPosition[y];

            /* Erase the bombs position */
           bombRect.xMax = bombs[i][x] + 2;
           bombRect.xMin = bombs[i][x] - 2;
           bombRect.yMax = bombs[i][y] + 2;
           bombRect.yMin = bombs[i][y] - 2;
           Graphics_fillRectangleOnDisplay(g_sContext.display, &bombRect,
                                           g_sContext.background);
           signal(&graphicsSemaphore, &drawBombsGraphicsCaller);
            /* Reset the bombs */
            bombs[i][x] = 200;
            bombs[i][y] = 200;
            /* Store the bombs position */
            previousbombs[i][x] = bombs[i][x];
            previousbombs[i][y] = bombs[i][y];
          }
       }
     }
   }
}

void drawSpacecraft(void)
{
  while (1)
  {
    if (ADC12_A_getInterruptStatus(ADC12_A_BASE, ADC12_A_IFG1))
    {
      if (!spacecraftIsDestroyed)
      {
        /* Store the spacecrafts previous position */
        previousSpacecraftPosition[x] = spacecraftPosition[x];
        previousSpacecraftPosition[y] = spacecraftPosition[y];
        /* Store the spacecrafts new position */
        spacecraftPosition[x] = mapValToRange(
            (uint32_t) ADC12_A_getResults(ADC12_A_BASE, ADC12_A_MEMORY_0), 0UL,
            255UL, 0UL, 127UL);
        /* Invert the y values so when the joystick goes up y goes down */
        spacecraftPosition[y] = 0x7F
            & ~mapValToRange(
                (uint32_t) ADC12_A_getResults(ADC12_A_BASE, ADC12_A_MEMORY_1),
                0UL, 255UL, 0UL, 127UL);
        /* If the spacecraft is in a new position clear its old position and redraw it */
        if (previousSpacecraftPosition[x] != spacecraftPosition[x]
            || previousSpacecraftPosition[y] != spacecraftPosition[y])
        {
          wait(&graphicsSemaphore, &drawSpacecraftGraphicsCaller);
          /* Clear the spacecrafts previous position */
          spacecraftRect.xMax = previousSpacecraftPosition[x] + 2;
          spacecraftRect.xMin = previousSpacecraftPosition[x] - 2;
          spacecraftRect.yMax = previousSpacecraftPosition[y] + 2;
          spacecraftRect.yMin = previousSpacecraftPosition[y] - 2;
          Graphics_fillRectangleOnDisplay(g_sContext.display, &spacecraftRect,
                                          g_sContext.background);
          /* Draw it's new position */
          spacecraftRect.xMax = spacecraftPosition[x] + 2;
          spacecraftRect.xMin = spacecraftPosition[x] - 2;
          spacecraftRect.yMax = spacecraftPosition[y] + 2;
          spacecraftRect.yMin = spacecraftPosition[y] - 2;
          Graphics_fillRectangle(&g_sContext, &spacecraftRect);
          signal(&graphicsSemaphore, &drawSpacecraftGraphicsCaller);
        }
      }
    }
  }
}

#pragma vector=PORT1_VECTOR
__interrupt void pushButtonISR(void)
{
  switch (__even_in_range(P1IV, P1IV_P1IFG1))
  {
    case P1IV_P1IFG1:
      // WDTCTL = 0xDEAD; // watchdog timer password violation causes a power up clear reset
      PMMCTL0 = PMMPW | PMMSWBOR; // Software brown out reset
      break;
    default:
      break;
  }
}
