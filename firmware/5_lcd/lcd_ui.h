/* Module 5 (pages): what each LCD1602 shows (proposal 1.3.3, SPEC section 6).
 * The LCD sits on the divider facing its player.
 *
 *   S1  SELECT MODE      / > 1 PLAYER        (P2: WAIT FOR P1)
 *   S2  PLACE SHIPS 3/6  / L3:1 L2:2 L1:0    (after CONFIRM: READY WAIT P2)
 *   S3  GAME START       / YOU FIRST
 *   S4  YOUR TURN    R3  / YOU C3 HIT        (last shot, any side)
 *   S5  YOU WIN!         / SHOT 23 HIT 48%  (or YOU GAVE UP / ENEMY GAVE UP)
 *   S6  RESETTING...
 *   1-player mode, AI side: 1 PLAYER MODE / AI SIDE
 *
 * HAL-free: formats the text, then writes through Lcd1602_WriteLine only the
 * lines that changed since the last call.
 */
#ifndef LCD_UI_H
#define LCD_UI_H

#include <stdint.h>

#include "game.h"

#define LCD_UI_COLS 16

void LcdUi_Init(void);
/* Updates both LCDs. Returns how many lines were rewritten. */
int LcdUi_Render(const Game *g);
/* Text for one player's LCD (buffers of LCD_UI_COLS + 1, NUL-terminated). */
void LcdUi_Format(const Game *g, uint8_t player,
                  char line0[LCD_UI_COLS + 1], char line1[LCD_UI_COLS + 1]);

#endif
