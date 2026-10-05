/* Bench test for the WS2812 driver alone (no game, no keypad).
 * Cycles through patterns that each check one thing:
 *   1-3  all RED / GREEN / BLUE   -> color order (G-R-B) is right
 *   4    all WHITE                -> power supply + 40% brightness cap
 *   5    PANEL ID                 -> CH1 red, CH2 green, CH3 blue, CH4 yellow
 *   6    WALK                     -> one LED moves A1, B1 ... H1, A2 ... H7
 *                                    (checks chain order / serpentine / flip)
 * HAL-free: call Ws2812Test_Loop(HAL_GetTick()) from while (1).
 */
#ifndef WS2812_TEST_H
#define WS2812_TEST_H

#include <stdint.h>

void Ws2812Test_Init(void);
void Ws2812Test_Loop(uint32_t now_ms);

/* Current pattern number 1..6 (handy to watch in the debugger). */
uint8_t Ws2812Test_Pattern(void);

#endif
