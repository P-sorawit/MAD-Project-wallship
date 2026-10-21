/* Renders every touch-screen layout to BMP files so the UI can be reviewed
 * on a PC. Build: render_screens.ps1. Uses the real game + UI code. */
#include "game.h"
#include "gfx.h"
#include "tft_ui.h"

#include <stdio.h>
#include <string.h>

static void save_bmp(const char *path)
{
    FILE *f = fopen(path, "wb");
    const uint32_t row = GFX_W * 3U, size = 54U + row * GFX_H;
    uint8_t h[54] = {'B', 'M'};
    int x, y;
    if (f == NULL) {
        perror(path);
        return;
    }
    h[2] = (uint8_t)size; h[3] = (uint8_t)(size >> 8); h[4] = (uint8_t)(size >> 16);
    h[10] = 54; h[14] = 40;
    h[18] = (uint8_t)GFX_W; h[19] = (uint8_t)(GFX_W >> 8);
    h[22] = (uint8_t)GFX_H; h[23] = (uint8_t)(GFX_H >> 8);
    h[26] = 1; h[28] = 24;
    fwrite(h, 1, sizeof h, f);
    for (y = GFX_H - 1; y >= 0; --y) {
        for (x = 0; x < GFX_W; ++x) {
            const uint16_t s = gfx_fb[y][x];
            const uint16_t v = (uint16_t)((s >> 8) | (s << 8));
            const uint8_t px[3] = {
                (uint8_t)((v & 0x1FU) << 3),
                (uint8_t)(((v >> 5) & 0x3FU) << 2),
                (uint8_t)(((v >> 11) & 0x1FU) << 3)
            };
            fwrite(px, 1, 3, f);
        }
    }
    fclose(f);
}

static void shot(Game *g, uint8_t p, const char *name)
{
    TftView v;
    char path[128];
    TftUi_BuildView(g, p, &v);
    TftUi_Draw(&v);
    snprintf(path, sizeof path, "%s.bmp", name);
    save_bmp(path);
    printf("wrote %s\n", path);
}

/* Partial-update demo: which 16-row bands differ between two frames. */
static uint16_t prev_fb[GFX_H][GFX_W];

static uint32_t diff_bands(void)
{
    uint32_t mask = 0;
    int b;
    for (b = 0; b < GFX_BANDS; ++b) {
        if (memcmp(prev_fb[b * GFX_BAND_H], gfx_fb[b * GFX_BAND_H],
                   sizeof(uint16_t) * GFX_W * GFX_BAND_H) != 0) {
            mask |= 1UL << b;
        }
    }
    return mask;
}

/* Darken bands that are NOT sent, outline the ones that are. */
static void mark_bands(uint32_t mask)
{
    int b, x, y;
    for (b = 0; b < GFX_BANDS; ++b) {
        const int y0 = b * GFX_BAND_H;
        if ((mask >> b) & 1U) {
            Gfx_Frame(0, y0, GFX_W, GFX_BAND_H, 1, 0xFF2020U);
            continue;
        }
        for (y = y0; y < y0 + GFX_BAND_H; ++y) {
            for (x = 0; x < GFX_W; ++x) {
                const uint16_t sw = gfx_fb[y][x];
                const uint16_t v = (uint16_t)((sw >> 8) | (sw << 8));
                const unsigned r = (v >> 11) & 0x1FU, gr = (v >> 5) & 0x3FU, bl = v & 0x1FU;
                const uint16_t d = (uint16_t)(((r / 4U) << 11) | ((gr / 4U) << 5) | (bl / 4U));
                gfx_fb[y][x] = (uint16_t)((d >> 8) | (d << 8));
            }
        }
    }
}

static const uint8_t fleet[6][4] = { /* len, x, y, vertical */
    {3, 0, 0, 0}, {3, 7, 0, 1}, {2, 0, 2, 0}, {2, 4, 4, 1}, {1, 2, 6, 0}, {1, 6, 6, 0}
};

static void place_ship(Game *g, uint8_t p, int s)
{
    uint8_t c;
    Game_OnButton(g, p, (GameButton)(BTN_SHIP_1 + fleet[s][0] - 1));
    for (c = 0; c < fleet[s][0]; ++c) {
        Game_OnKey(g, GAME_FLEET_PANEL(p), (uint8_t)(fleet[s][1] + (fleet[s][3] ? 0 : c)),
                   (uint8_t)(fleet[s][2] + (fleet[s][3] ? c : 0)));
    }
}

int main(void)
{
    Game g;
    uint32_t now = 1000;
    int s;
    uint8_t x = 0, y = 0;
    memset(&g, 0, sizeof g);
    Game_Init(&g, 42, AI_BALANCED);
    shot(&g, 0, "01_home");
    shot(&g, 1, "02_home_wait_p2");

    Game_OnButton(&g, 0, BTN_MODE_1P);
    Game_OnButton(&g, 0, BTN_CONFIRM);
    for (s = 0; s < 4; ++s) {
        place_ship(&g, 0, s);
    }
    Game_OnButton(&g, 0, BTN_SHIP_1);
    shot(&g, 0, "03_ships");
    shot(&g, 1, "04_ai_side");
    for (; s < 6; ++s) {
        place_ship(&g, 0, s);
    }
    Game_OnButton(&g, 0, BTN_CONFIRM);
    shot(&g, 0, "05_ready_start");

    /* play until a few ships are down on both sides */
    now += 2000;
    Game_Tick(&g, now);
    while (g.state == GS_BATTLE && (g.shots[0] < 18 || g.match.current_player != 0)) {
        if (g.match.current_player == 0) {
            Game_OnKey(&g, PANEL_P1_ATTACK, x, y);
            if (++x == BOARD_W) { x = 0; ++y; }
        } else {
            now += 1000;
            Game_Tick(&g, now);
        }
    }
    shot(&g, 0, "06_battle");

    /* demo: one more shot by P1, compare the two frames band by band */
    {
        TftView v;
        uint32_t mask;
        int n = 0, b;
        memcpy(prev_fb, gfx_fb, sizeof prev_fb);
        Game_OnKey(&g, PANEL_P1_ATTACK, x, y);
        if (++x == BOARD_W) { x = 0; ++y; }
        TftUi_BuildView(&g, 0, &v);
        TftUi_Draw(&v);
        save_bmp("09_after_shot.bmp");
        mask = diff_bands();
        for (b = 0; b < GFX_BANDS; ++b) {
            n += (int)((mask >> b) & 1U);
        }
        mark_bands(mask);
        save_bmp("10_bands_sent.bmp");
        printf("partial update: %d/%d bands sent (mask 0x%05lX)\n", n, GFX_BANDS,
               (unsigned long)mask);
    }

    while (g.state == GS_BATTLE) {
        if (g.match.current_player == 0) {
            Game_OnKey(&g, PANEL_P1_ATTACK, x, y);
            if (++x == BOARD_W) { x = 0; ++y; }
        } else {
            now += 1000;
            Game_Tick(&g, now);
        }
    }
    shot(&g, 0, g.winner == 0 ? "07_victory" : "07_defeat");
    /* the other result screen, from the same finished match */
    g.winner = (uint8_t)(1U - g.winner);
    shot(&g, 0, g.winner == 0 ? "08_victory" : "08_defeat");
    return 0;
}
