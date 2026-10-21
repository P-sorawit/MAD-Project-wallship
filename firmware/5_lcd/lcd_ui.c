#include "lcd_ui.h"

#include "lcd1602.h"

#include <stdio.h>
#include <string.h>

#define N (LCD_UI_COLS + 1)

static char shown[GAME_PLAYERS][2][N];
static int shown_valid[GAME_PLAYERS];

void LcdUi_Init(void)
{
    memset(shown, 0, sizeof shown);
    memset(shown_valid, 0, sizeof shown_valid);
}

static uint8_t ships_left(const WallshipAgent *a)
{
    uint8_t i, n = 0;
    for (i = 0; i < SHIP_COUNT; ++i) {
        n = (uint8_t)(n + (a->ships[i].sunk ? 0U : 1U));
    }
    return n;
}

static const char *result_text(ShotResult r)
{
    return r == SHOT_MISS ? "MISS" : (r == SHOT_HIT ? "HIT" : "SUNK");
}

void LcdUi_Format(const Game *g, uint8_t p, char l0[N], char l1[N])
{
    const uint8_t e = (uint8_t)(1U - p);
    const char *enemy = Game_IsHuman(g, e) ? "ENEMY" : "AI";
    l0[0] = '\0';
    l1[0] = '\0';

    if (g->state != GS_SELECT_MODE && g->state != GS_RESET && !Game_IsHuman(g, p)) {
        snprintf(l0, N, "1 PLAYER MODE");
        snprintf(l1, N, "AI SIDE");
        return;
    }
    switch (g->state) {
    case GS_SELECT_MODE:
        snprintf(l0, N, "SELECT MODE");
        if (p == 0) {
            snprintf(l1, N, "> %s", g->mode_cursor == MODE_1P ? "1 PLAYER" : "2 PLAYERS");
        } else {
            snprintf(l1, N, "WAIT FOR P1");
        }
        break;

    case GS_PLACE_SHIPS: {
        const Placement *pl = &g->placing[p];
        snprintf(l0, N, "PLACE SHIPS %u/6", (unsigned)(pl->placed % 10U));
        if (g->ready[p]) {
            snprintf(l1, N, "READY WAIT P%u", (unsigned)(e % 2U) + 1U);
        } else {
            snprintf(l1, N, "L3:%u L2:%u L1:%u", (unsigned)(pl->remaining[3] % 10U),
                     (unsigned)(pl->remaining[2] % 10U), (unsigned)(pl->remaining[1] % 10U));
        }
        break;
    }

    case GS_GAME_START:
        snprintf(l0, N, "GAME START");
        snprintf(l1, N, "%s FIRST", g->first_player == p ? "YOU" : enemy);
        break;

    case GS_BATTLE: {
        /* turn on the left, round right-aligned: "YOUR TURN    R12" */
        const char *turn = g->match.current_player == p ? "YOUR TURN"
                         : (Game_IsHuman(g, e) ? "ENEMY TURN" : "AI TURN");
        char r[8];
        size_t rlen;
        snprintf(r, sizeof r, "R%u", (unsigned)(g->round % 1000U));
        rlen = strlen(r);
        memset(l0, ' ', LCD_UI_COLS);
        l0[LCD_UI_COLS] = '\0';
        memcpy(l0, turn, strlen(turn));
        memcpy(l0 + LCD_UI_COLS - rlen, r, rlen);
        if (g->has_last_shot) {
            snprintf(l1, N, "%s %c%u %s", g->last_shooter == p ? "YOU" : enemy,
                     'A' + g->last_shot.x % 8U, (unsigned)(g->last_shot.y % 8U) + 1U,
                     result_text(g->last_result));
        } else {
            snprintf(l1, N, "SHIPS %u VS %u", (unsigned)(ships_left(&g->player[p]) % 10U),
                     (unsigned)(ships_left(&g->player[e]) % 10U));
        }
        break;
    }

    case GS_RESULT:
        snprintf(l0, N, g->winner == p ? "YOU WIN!" : "YOU LOSE");
        if (g->surrendered) {
            snprintf(l1, N, "%s GAVE UP", g->winner == p ? enemy : "YOU");
        } else {
            const unsigned shots = (unsigned)(g->shots[p] % 100U);
            const unsigned pct = g->shots[p] != 0U
                ? (unsigned)(g->hits[p] * 100U / g->shots[p]) % 101U : 0U;
            snprintf(l1, N, "SHOT %u HIT %u%%", shots, pct);
        }
        break;

    case GS_RESET:
        snprintf(l0, N, "RESETTING...");
        break;
    }
}

int LcdUi_Render(const Game *g)
{
    uint8_t p, row;
    int written = 0;
    for (p = 0; p < GAME_PLAYERS; ++p) {
        char lines[2][N];
        LcdUi_Format(g, p, lines[0], lines[1]);
        for (row = 0; row < 2U; ++row) {
            if (!shown_valid[p] || strcmp(lines[row], shown[p][row]) != 0) {
                Lcd1602_WriteLine(p, row, lines[row]);
                memcpy(shown[p][row], lines[row], N);
                ++written;
            }
        }
        shown_valid[p] = 1;
    }
    return written;
}
