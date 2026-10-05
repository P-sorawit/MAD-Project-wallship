/* Glue between CubeMX main.c and the modules.
 * In main.c (USER CODE sections only):
 *   USER CODE BEGIN Includes : #include "app.h"
 *   USER CODE BEGIN 2        : App_Init();
 *   USER CODE BEGIN 3        : App_Loop();   (inside while (1))
 */
#ifndef APP_H
#define APP_H

#include <stdint.h>

#include "game.h"

void App_Init(void);
void App_Loop(void);
/* Main-loop context only: feed a button press from any input module. */
void App_OnButton(uint8_t player, GameButton button);

#endif
