/* Module 2 (part): turn game state into panel colors (proposal 1.3.4).
 *   water          blue 100%     own ship        white
 *   missed water   blue 15%      hit ship cell   orange
 *   sunk ship      red           S2 pending cell dim white
 * HAL-free; only calls Ws2812_SetPixel. Call Ws2812_Show() afterwards.
 */
#ifndef LED_VIEW_H
#define LED_VIEW_H

#include "game.h"
#include "ws2812.h"

extern const Rgb LED_OFF, LED_WATER, LED_MISS, LED_SHIP, LED_HIT, LED_SUNK, LED_PENDING;

void LedView_Render(const Game *g);

#endif
