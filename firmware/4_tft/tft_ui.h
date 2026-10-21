/* Module 4 (screens): what each player's touch screen shows, 240x320 portrait.
 *
 *   Home   (S1)      title + battleship art, menu: 1. SINGLE PLAYER / 2. TWO PLAYERS
 *   Ships  (S2)      SELECT SHIPS: [1x1] x n, [2x1] x n, [3x1] x n (real counts),
 *                    CONFIRM / CANCEL
 *   Battle (S3, S4)  GAME STATUS / BATTLE!, your ships left (icons), enemy targets,
 *                    shots, round, whose turn, sound on/off
 *   Defeat / Victory (S5)  title + art + stats, menu: RETRY|NEXT GAME / MAIN MENU
 *
 * HAL-free: builds a small "view" from the Game, redraws only when the view
 * changed, and sends only the changed bands through Tft_SendBands (tft.h).
 */
#ifndef TFT_UI_H
#define TFT_UI_H

#include <stdbool.h>
#include <stdint.h>

#include "game.h"

typedef enum {
    SCR_NONE = 0,
    SCR_HOME,        /* P1 chooses the mode */
    SCR_HOME_WAIT,   /* P2 while P1 chooses */
    SCR_AI,          /* player 2 side in single-player mode */
    SCR_SHIPS,
    SCR_SHIPS_READY,
    SCR_BATTLE,
    SCR_DEFEAT,
    SCR_VICTORY,
    SCR_RESET
} TftScreen;

/* Everything a screen shows. Two equal views draw identical pixels. */
typedef struct {
    uint8_t screen;          /* TftScreen */
    uint8_t mode_cursor;     /* 0 = 1P, 1 = 2P */
    uint8_t remaining[4];    /* index = ship length */
    uint8_t selected_len;
    uint8_t placed;
    uint8_t complete;
    uint8_t starting;        /* S3 */
    uint8_t my_turn;
    uint8_t first_is_me;
    uint8_t enemy_is_ai;
    uint8_t my_left, enemy_left;
    uint8_t my_sunk_mask;    /* bit i = ships[i] sunk (lengths 3,3,2,2,1,1) */
    uint8_t muted;
    uint8_t surrendered;
    uint8_t last_valid, last_by_me, last_x, last_y, last_result;
    uint16_t shots, hits, round;
} TftView;

/* Commands a tap produces (up to two, e.g. "select 1P" then "start"). */
typedef struct {
    GameButton first, second;
} TftAction;

void TftUi_Init(void);
void TftUi_BuildView(const Game *g, uint8_t player, TftView *v);
void TftUi_Draw(const TftView *v);              /* into gfx_fb */
TftAction TftUi_HitTest(const Game *g, uint8_t player, int x, int y);

/* Non-blocking. If the display link is idle and a player's view changed,
 * draws that screen into gfx_fb and starts DMA for only the 16-row bands that
 * differ from what the display shows. One display per call; call every loop.
 * Returns 1 if a transfer was started. */
int TftUi_Render(const Game *g);
uint32_t TftUi_LastBandMask(void);   /* bands sent by the last transfer (tests) */

#endif
