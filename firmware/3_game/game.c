#include "game.h"

#include "ai_player.h"
#include "buzzer.h"

#include <string.h>

#ifndef GAME_START_SHOW_MS
#define GAME_START_SHOW_MS 2000U  /* S3 "GAME START" banner */
#endif
#ifndef GAME_AI_SHOT_DELAY_MS
#define GAME_AI_SHOT_DELAY_MS 900U /* so humans can follow each AI shot */
#endif
#ifndef GAME_RESET_SHOW_MS
#define GAME_RESET_SHOW_MS 1000U  /* S6 "RESETTING" */
#endif

static uint32_t next_random(Game *g)
{
    uint32_t x = g->rng != 0 ? g->rng : WALLSHIP_DEFAULT_SEED;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g->rng = x;
    return x;
}

static void enter_state(Game *g, GameState s)
{
    g->state = s;
    g->state_since_ms = g->now_ms;
    g->dirty = true;
}

bool Game_IsHuman(const Game *g, uint8_t player)
{
    return g->mode == MODE_2P || player == 0;
}

uint8_t Game_ShipsLeft(const Game *g, uint8_t player)
{
    uint8_t i, left = 0;
    for (i = 0; i < SHIP_COUNT; ++i) {
        if (!g->player[player].ships[i].sunk) {
            ++left;
        }
    }
    return left;
}

void Game_Init(Game *g, uint32_t seed, AIProfile ai_profile)
{
    const uint32_t now = g->now_ms;
    memset(g, 0, sizeof *g);
    g->now_ms = now;
    g->rng = seed != 0 ? seed : WALLSHIP_DEFAULT_SEED;
    g->ai_profile = ai_profile;
    g->mode_cursor = MODE_1P;
    g->winner = INVALID_COORD;
    enter_state(g, GS_SELECT_MODE);
}

static void start_placement(Game *g)
{
    uint8_t p;
    g->mode = g->mode_cursor;
    g->rng ^= g->now_ms * UINT32_C(2654435761); /* human timing adds entropy */
    for (p = 0; p < GAME_PLAYERS; ++p) {
        g->ready[p] = false;
        if (Game_IsHuman(g, p)) {
            WallshipAI_Init(&g->player[p], next_random(g), AI_RANDOM);
            Placement_Clear(&g->placing[p], &g->player[p]);
        } else if (AiPlayer_Setup(&g->player[p], next_random(g), g->ai_profile)) {
            g->ready[p] = true;
        }
        /* If AI placement ever fails, ready stays false and S2 never ends;
         * PlaceShips failure is practically impossible on 8x7 with 12 cells. */
    }
    enter_state(g, GS_PLACE_SHIPS);
}

static void start_match(Game *g)
{
    g->first_player = (uint8_t)(next_random(g) & 1U);
    if (!WallshipTwoPlayerGame_Init(&g->match, &g->player[0], &g->player[1],
                                    g->first_player)) {
        start_placement(g); /* inconsistent fleet: should not happen */
        return;
    }
    g->has_last_shot = false;
    g->surrendered = false;
    g->ai_next_shot_ms = g->now_ms + GAME_START_SHOW_MS + GAME_AI_SHOT_DELAY_MS;
    Buzzer_PlaySound(BUZZ_BOTH, SND_START);
    enter_state(g, GS_GAME_START);
}

static void finish(Game *g, uint8_t winner, bool surrendered)
{
    g->winner = winner;
    g->surrendered = surrendered;
    Buzzer_PlaySound(BUZZ_BOTH, SND_GAME_OVER);
    enter_state(g, GS_RESULT);
}

static void record_shot(Game *g, uint8_t shooter, Position p, ShotResult r,
                        uint8_t sunk_length)
{
    g->has_last_shot = true;
    g->last_shooter = shooter;
    g->last_shot = p;
    g->last_result = r;
    g->last_sunk_length = sunk_length;
    g->dirty = true;
    Buzzer_PlaySound(BUZZ_BOTH, r == SHOT_MISS ? SND_MISS
                                : (r == SHOT_HIT ? SND_HIT : SND_SUNK));
    if (g->match.finished) {
        finish(g, g->match.winner, false);
    } else if (!Game_IsHuman(g, g->match.current_player)) {
        g->ai_next_shot_ms = g->now_ms + GAME_AI_SHOT_DELAY_MS;
    }
}

static void on_place_key(Game *g, uint8_t p, uint8_t x, uint8_t y)
{
    switch (Placement_PressCell(&g->placing[p], &g->player[p], x, y)) {
    case PLACE_PENDING:
    case PLACE_SHIP_DONE:
        Buzzer_PlaySound(BUZZ_PLAYER(p), SND_CLICK);
        g->dirty = true;
        break;
    default:
        Buzzer_PlaySound(BUZZ_PLAYER(p), SND_ERROR);
        break;
    }
}

static void on_attack_key(Game *g, uint8_t p, uint8_t x, uint8_t y)
{
    uint8_t sunk_length = 0;
    ShotResult r;
    Position pos;
    if (p != g->match.current_player) {
        return; /* not your turn: ignore silently */
    }
    r = WallshipTwoPlayerGame_Shoot(&g->match, p, x, y, &sunk_length);
    if (r == SHOT_INVALID) {
        Buzzer_PlaySound(BUZZ_PLAYER(p), SND_ERROR); /* already fired here */
        return;
    }
    pos.x = x;
    pos.y = y;
    record_shot(g, p, pos, r, sunk_length);
}

void Game_OnKey(Game *g, uint8_t panel, uint8_t x, uint8_t y)
{
    const uint8_t p = (uint8_t)(panel / 2U);
    const bool attack_panel = (panel & 1U) != 0;
    if (panel >= PANEL_COUNT || x >= BOARD_W || y >= BOARD_H || !Game_IsHuman(g, p)) {
        return;
    }
    if (g->state == GS_PLACE_SHIPS && !attack_panel && !g->ready[p]) {
        on_place_key(g, p, x, y);
    } else if (g->state == GS_BATTLE && attack_panel) {
        on_attack_key(g, p, x, y);
    }
}

void Game_OnButton(Game *g, uint8_t p, GameButton b)
{
    if (p >= GAME_PLAYERS) {
        return;
    }
    switch (g->state) {
    case GS_SELECT_MODE:
        if (p != 0) {
            break; /* player 1 chooses the mode */
        }
        if (b == BTN_MODE_1P || b == BTN_MODE_2P) {
            g->mode_cursor = b == BTN_MODE_1P ? MODE_1P : MODE_2P;
            Buzzer_PlaySound(BUZZ_P1, SND_CLICK);
            g->dirty = true;
        } else if (b == BTN_CONFIRM) {
            Buzzer_PlaySound(BUZZ_BOTH, SND_CLICK);
            start_placement(g);
        }
        break;

    case GS_PLACE_SHIPS:
        if (!Game_IsHuman(g, p) || g->ready[p]) {
            break;
        }
        if (b == BTN_SHIP_1 || b == BTN_SHIP_2 || b == BTN_SHIP_3) {
            const uint8_t len = (uint8_t)(b - BTN_SHIP_1 + 1);
            Buzzer_PlaySound(BUZZ_PLAYER(p), Placement_SelectLength(&g->placing[p], len)
                             ? SND_CLICK : SND_ERROR);
            g->dirty = true;
        } else if (b == BTN_CANCEL) {
            Placement_Clear(&g->placing[p], &g->player[p]);
            Buzzer_PlaySound(BUZZ_PLAYER(p), SND_CLICK);
            g->dirty = true;
        } else if (b == BTN_CONFIRM) {
            if (!Placement_IsComplete(&g->placing[p])) {
                Buzzer_PlaySound(BUZZ_PLAYER(p), SND_ERROR);
                break;
            }
            g->ready[p] = true;
            g->dirty = true;
            if (g->ready[0] && g->ready[1]) {
                start_match(g);
            } else {
                Buzzer_PlaySound(BUZZ_PLAYER(p), SND_CLICK);
            }
        }
        break;

    case GS_GAME_START:
    case GS_BATTLE:
        if (b == BTN_SURRENDER && Game_IsHuman(g, p)) {
            finish(g, (uint8_t)(1U - p), true);
        }
        break;

    case GS_RESULT:
        if (b == BTN_CONFIRM && Game_IsHuman(g, p)) {
            enter_state(g, GS_RESET);
        }
        break;

    default:
        break;
    }
}

void Game_Tick(Game *g, uint32_t now_ms)
{
    g->now_ms = now_ms;
    switch (g->state) {
    case GS_GAME_START:
        if (now_ms - g->state_since_ms >= GAME_START_SHOW_MS) {
            enter_state(g, GS_BATTLE);
        }
        break;

    case GS_BATTLE: {
        const uint8_t cur = g->match.current_player;
        if (!Game_IsHuman(g, cur) && (int32_t)(now_ms - g->ai_next_shot_ms) >= 0) {
            Position p;
            uint8_t sunk_length;
            const ShotResult r = AiPlayer_TakeShot(&g->player[cur], &g->match, cur,
                                                   &p, &sunk_length);
            if (r == SHOT_INVALID) {
                finish(g, (uint8_t)(1U - cur), false); /* should never happen */
            } else {
                record_shot(g, cur, p, r, sunk_length);
            }
        }
        break;
    }

    case GS_RESET:
        if (now_ms - g->state_since_ms >= GAME_RESET_SHOW_MS) {
            Game_Init(g, next_random(g), g->ai_profile);
        }
        break;

    default:
        break;
    }
}

bool Game_TakeDirty(Game *g)
{
    const bool d = g->dirty;
    g->dirty = false;
    return d;
}
