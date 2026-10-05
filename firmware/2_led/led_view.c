#include "led_view.h"

/* Full-scale values; ws2812.c applies the 40% brightness cap on top. */
const Rgb LED_OFF     = {0, 0, 0};
const Rgb LED_WATER   = {0, 0, 255};
const Rgb LED_MISS    = {0, 0, 38};     /* 15% */
const Rgb LED_SHIP    = {255, 255, 255};
const Rgb LED_HIT     = {255, 80, 0};
const Rgb LED_SUNK    = {255, 0, 0};
const Rgb LED_PENDING = {80, 80, 80};

/* Color of a cell on `defender`'s board as seen after shots.
 * show_ships: draw unshot ships white (own panel, or reveal in S5). */
static Rgb cell_color(const WallshipAgent *defender, uint8_t x, uint8_t y,
                      bool show_ships)
{
    const uint8_t id = defender->own_board[y][x];
    if (defender->human_shot_board[y][x] != 0) {
        if (id == 0) {
            return LED_MISS;
        }
        return defender->ships[id - 1].sunk ? LED_SUNK : LED_HIT;
    }
    return (id != 0 && show_ships) ? LED_SHIP : LED_WATER;
}

void LedView_Render(const Game *g)
{
    uint8_t p, x, y;
    for (p = 0; p < GAME_PLAYERS; ++p) {
        const uint8_t fleet = GAME_FLEET_PANEL(p);
        const uint8_t attack = GAME_ATTACK_PANEL(p);
        const WallshipAgent *own = &g->player[p];
        const WallshipAgent *enemy = &g->player[1U - p];
        for (y = 0; y < BOARD_H; ++y) {
            for (x = 0; x < BOARD_W; ++x) {
                Rgb f = LED_OFF, a = LED_OFF;
                switch (g->state) {
                case GS_PLACE_SHIPS:
                    if (own->own_board[y][x] != 0) {
                        f = LED_SHIP;
                    } else if (Game_IsHuman(g, p) &&
                               Placement_IsPending(&g->placing[p], x, y)) {
                        f = LED_PENDING;
                    } else {
                        f = LED_WATER;
                    }
                    break;
                case GS_GAME_START:
                case GS_BATTLE:
                    f = cell_color(own, x, y, true);
                    a = cell_color(enemy, x, y, false);
                    break;
                case GS_RESULT: /* reveal every ship on both sides */
                    f = cell_color(own, x, y, true);
                    a = cell_color(enemy, x, y, true);
                    break;
                default: /* S1, S6: all off */
                    break;
                }
                Ws2812_SetPixel(fleet, x, y, f);
                Ws2812_SetPixel(attack, x, y, a);
            }
        }
    }
}
