/* Module 3: main game state machine (S1-S6 from the proposal).
 * HAL-free: input arrives through Game_OnKey/Game_OnButton/Game_Tick and the
 * LED view reads the Game struct when Game_TakeDirty() returns true.
 */
#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "wallship_ai.h"
#include "placement.h"

#define GAME_PLAYERS 2

/* Logical panels. keypad.c / ws2812 map physical wiring onto these. */
typedef enum {
    PANEL_P1_FLEET = 0,
    PANEL_P1_ATTACK = 1,
    PANEL_P2_FLEET = 2,
    PANEL_P2_ATTACK = 3,
    PANEL_COUNT = 4
} GamePanel;

#define GAME_FLEET_PANEL(p)  ((uint8_t)((p) * 2U))
#define GAME_ATTACK_PANEL(p) ((uint8_t)((p) * 2U + 1U))

typedef enum {
    GS_SELECT_MODE = 1, /* S1 */
    GS_PLACE_SHIPS,     /* S2 */
    GS_GAME_START,      /* S3 */
    GS_BATTLE,          /* S4 */
    GS_RESULT,          /* S5 */
    GS_RESET            /* S6 */
} GameState;

typedef enum { MODE_1P = 0, MODE_2P } GameMode;

/* Button commands, independent of the input device (see App_OnButton). */
typedef enum {
    BTN_NONE = 0,
    BTN_MODE_1P,
    BTN_MODE_2P,
    BTN_SHIP_1,
    BTN_SHIP_2,
    BTN_SHIP_3,
    BTN_CANCEL,
    BTN_CONFIRM,   /* S1 start, S2 confirm fleet, S5 new game */
    BTN_SURRENDER
} GameButton;

typedef struct {
    GameState state;
    GameMode mode;
    GameMode mode_cursor;
    AIProfile ai_profile;
    uint32_t now_ms;
    uint32_t state_since_ms;
    uint32_t ai_next_shot_ms;
    uint32_t rng;

    WallshipAgent player[GAME_PLAYERS]; /* player[1] is the AI brain in 1P */
    Placement placing[GAME_PLAYERS];
    bool ready[GAME_PLAYERS];
    WallshipTwoPlayerGame match;

    uint8_t first_player;
    bool has_last_shot;
    uint8_t last_shooter;
    Position last_shot;
    ShotResult last_result;
    uint8_t last_sunk_length;

    uint8_t winner;     /* valid in S5 */
    bool surrendered;
    bool dirty;
} Game;

void Game_Init(Game *g, uint32_t seed, AIProfile ai_profile);
void Game_OnKey(Game *g, uint8_t panel, uint8_t x, uint8_t y);
void Game_OnButton(Game *g, uint8_t player, GameButton button);
void Game_Tick(Game *g, uint32_t now_ms);

/* Returns true once after any visible change, then clears the flag. */
bool Game_TakeDirty(Game *g);

bool Game_IsHuman(const Game *g, uint8_t player);
uint8_t Game_ShipsLeft(const Game *g, uint8_t player);

#endif
