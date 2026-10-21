#include "tft_ui.h"

#include "art.h"
#include "buzzer.h"
#include "gfx.h"
#include "tft.h"

#include <stdio.h>
#include <string.h>

/* ---------------- palette ---------------- */
#define C_BLACK     0x000000U
#define C_WHITE     0xFFFFFFU
#define C_OUTLINE   0x0B1A44U
#define C_GOLD_T    0xFFF27AU
#define C_GOLD_B    0xFF8A1EU
#define C_NAVY      0x0C2150U
#define C_NAVY_2    0x15346FU
#define C_PANEL_LN  0x3E6CC4U
#define C_CYAN      0x7FD6FFU
#define C_ORANGE    0xFF9A2EU
#define C_GREY      0x6E7682U
#define C_MENU_LN   0x9AA3ADU

/* ---------------- layout (shared by drawing and hit-testing) ---------------- */
#define MENU_Y       250
#define MENU_LINE1   (MENU_Y + 4)
#define MENU_LINE2   (MENU_Y + 36)
#define SHIP_ROW_Y0  70
#define SHIP_ROW_H   50
#define SHIP_BAR_Y   262
#define BTN_W        108
#define BTN_H        44
#define BTN_L_X      8
#define BTN_R_X      124
#define BTN_Y        (SHIP_BAR_Y + 8)

static TftView shown[GAME_PLAYERS];
static bool shown_valid[GAME_PLAYERS];
/* What each display currently holds, one hash per 16-row band. */
static uint32_t band_hash[GAME_PLAYERS][GFX_BANDS];
static bool hash_valid[GAME_PLAYERS];
static uint8_t next_player;
static uint32_t last_mask;

void TftUi_Init(void)
{
    memset(shown, 0, sizeof shown);
    memset(shown_valid, 0, sizeof shown_valid);
    memset(hash_valid, 0, sizeof hash_valid);
    next_player = 0;
    last_mask = 0;
}

/* ---------------- view model ---------------- */

static uint8_t ships_left(const WallshipAgent *a)
{
    uint8_t i, n = 0;
    for (i = 0; i < SHIP_COUNT; ++i) {
        n = (uint8_t)(n + (a->ships[i].sunk ? 0U : 1U));
    }
    return n;
}

void TftUi_BuildView(const Game *g, uint8_t p, TftView *v)
{
    const uint8_t e = (uint8_t)(1U - p);
    uint8_t i;
    memset(v, 0, sizeof *v);
    v->mode_cursor = (uint8_t)g->mode_cursor;
    v->enemy_is_ai = (uint8_t)!Game_IsHuman(g, e);

    if (g->state == GS_RESET) {
        v->screen = SCR_RESET;
        return;
    }
    if (g->state == GS_SELECT_MODE) {
        v->screen = p == 0 ? SCR_HOME : SCR_HOME_WAIT;
        return;
    }
    if (!Game_IsHuman(g, p)) {
        v->screen = SCR_AI;
        return;
    }
    switch (g->state) {
    case GS_PLACE_SHIPS: {
        const Placement *pl = &g->placing[p];
        v->screen = g->ready[p] ? SCR_SHIPS_READY : SCR_SHIPS;
        for (i = 1; i <= 3; ++i) {
            v->remaining[i] = pl->remaining[i];
        }
        v->selected_len = pl->selected_length;
        v->placed = pl->placed;
        v->complete = (uint8_t)Placement_IsComplete(pl);
        break;
    }
    case GS_GAME_START:
    case GS_BATTLE:
    case GS_RESULT:
        v->screen = g->state != GS_RESULT ? SCR_BATTLE
                    : (g->winner == p ? SCR_VICTORY : SCR_DEFEAT);
        v->starting = (uint8_t)(g->state == GS_GAME_START);
        v->my_turn = (uint8_t)(g->match.current_player == p);
        v->first_is_me = (uint8_t)(g->first_player == p);
        v->my_left = ships_left(&g->player[p]);
        v->enemy_left = ships_left(&g->player[e]);
        for (i = 0; i < SHIP_COUNT; ++i) {
            if (g->player[p].ships[i].sunk) {
                v->my_sunk_mask = (uint8_t)(v->my_sunk_mask | (1U << i));
            }
        }
        v->muted = (uint8_t)Buzzer_IsMuted(p);
        v->surrendered = (uint8_t)g->surrendered;
        v->shots = g->shots[p];
        v->hits = g->hits[p];
        v->round = g->round;
        if (g->has_last_shot) {
            v->last_valid = 1;
            v->last_by_me = (uint8_t)(g->last_shooter == p);
            v->last_x = g->last_shot.x;
            v->last_y = g->last_shot.y;
            v->last_result = (uint8_t)g->last_result;
        }
        break;
    default:
        v->screen = SCR_RESET;
        break;
    }
}

/* ---------------- shared pieces ---------------- */

static void menu_panel(const char *line1, const char *line2, int arrow_line)
{
    char buf[24];
    Gfx_Rect(0, MENU_Y, GFX_W, GFX_H - MENU_Y, C_BLACK);
    Gfx_Frame(4, MENU_Y + 2, GFX_W - 8, GFX_H - MENU_Y - 6, 2, C_MENU_LN);
    snprintf(buf, sizeof buf, "%s%s", arrow_line == 1 ? "\x10 " : "  ", line1);
    Gfx_TextCenter(GFX_W / 2, MENU_LINE1 + 10, buf, 2, 2, C_WHITE);
    snprintf(buf, sizeof buf, "%s%s", arrow_line == 2 ? "\x10 " : "  ", line2);
    Gfx_TextCenter(GFX_W / 2, MENU_LINE2 + 8, buf, 2, 2, C_WHITE);
}

static void day_scene(void)
{
    Gfx_VGradient(0, 0, GFX_W, 196, 0x2C78D6U, 0xC3E7FFU, 9);
    Art_Cloud(14, 96, 3, 0xE8F5FFU);
    Art_Cloud(170, 118, 2, 0xE8F5FFU);
    Art_Birds(168, 74, 0x1B3A66U);
    Art_Sea(0, 196, GFX_W, MENU_Y - 196, 0x2B70C2U, 0x0E3A7AU, 0x9CD4FFU, 1);
    Art_Battleship(48, 222, 2, &SHIP_STEEL);
}

static void title(const char *s, int y, int scale)
{
    Gfx_TitleCenter(GFX_W / 2, y, s, scale, 3, C_GOLD_T, C_GOLD_B, C_OUTLINE, 2);
}

static void coord_text(char *out, size_t n, uint8_t x, uint8_t y)
{
    snprintf(out, n, "%c%u", 'A' + x % 8, (unsigned)(y % 8U + 1U));
}

/* ---------------- screens ---------------- */

static void draw_home(const TftView *v)
{
    day_scene();
    title("BATTLESHIP", 26, 4);
    if (v->screen == SCR_HOME) {
        menu_panel("1. SINGLE PLAYER", "2. TWO PLAYERS", v->mode_cursor == MODE_1P ? 1 : 2);
    } else {
        menu_panel("WAITING FOR", "PLAYER 1", 0);
    }
}

/* Single-player mode, screen on the AI's side: send the player around. */
static void draw_other_side(void)
{
    static const int stars[][2] = {{18, 40}, {70, 28}, {150, 34}, {210, 52}, {36, 98}, {196, 96}};
    int i;
    Gfx_VGradient(0, 0, GFX_W, 214, 0x0A1530U, 0x2A4A7AU, 8);
    for (i = 0; i < 6; ++i) {
        Gfx_Rect(stars[i][0], stars[i][1], 2, 2, 0xC8D4EAU);
    }
    Art_Sea(0, 214, GFX_W, MENU_Y - 214, 0x163A6AU, 0x0A1E3CU, 0x6A9ACAU, 7);
    Gfx_TitleCenter(GFX_W / 2, 12, "BATTLESHIP", 4, 3, 0xEAF6FFU, 0x5AA8E8U, C_OUTLINE, 2);
    Gfx_TextCenter(GFX_W / 2, 52, "SINGLE PLAYER", 2, 1, 0xFFD23AU);
    Gfx_TextCenter(GFX_W / 2, 70, "MODE ACTIVE", 2, 1, 0xFFD23AU);

    /* console on a table, arrow from this side over the divider */
    Art_Console(20, 204);
    Art_ArcArrow(62, 178, 150, 46, 0xFF8A1EU);

    /* this side: red cross */
    Gfx_Text(10, 112, "THIS SIDE", 1, 1, C_WHITE);
    Gfx_Line(26, 126, 40, 140, 0xFF3A2EU);
    Gfx_Line(27, 126, 41, 140, 0xFF3A2EU);
    Gfx_Line(40, 126, 26, 140, 0xFF3A2EU);
    Gfx_Line(41, 126, 27, 140, 0xFF3A2EU);

    /* opposite side: green bubble + person walking there */
    Gfx_Text(160, 98, "OPPOSITE SIDE", 1, 0, C_WHITE);
    Art_Panel(184, 110, 52, 30, 0x1E7A34U, 0x9CFF8AU);
    Gfx_TextCenter(210, 115, "PLAY", 1, 1, C_WHITE);
    Gfx_TextCenter(210, 126, "HERE!", 1, 1, C_WHITE);
    Art_Person(222, 238);

    /* message */
    Gfx_Rect(0, MENU_Y, GFX_W, GFX_H - MENU_Y, C_BLACK);
    Gfx_Frame(4, MENU_Y + 2, GFX_W - 8, GFX_H - MENU_Y - 6, 2, C_MENU_LN);
    Gfx_TextCenter(GFX_W / 2, MENU_Y + 10, "PLEASE GO TO", 2, 1, C_WHITE);
    Gfx_TextCenter(GFX_W / 2, MENU_Y + 30, "THE OPPOSITE SIDE", 2, 1, 0xFFD23AU);
    Gfx_TextCenter(GFX_W / 2, MENU_Y + 50, "TO PLAY", 2, 1, C_WHITE);
}

static void draw_ships(const TftView *v)
{
    char buf[24];
    int i;
    Gfx_Rect(0, 0, GFX_W, GFX_H, C_NAVY);
    title("SELECT SHIPS", 12, 3);
    Gfx_TextCenter(GFX_W / 2, 46, "CHOOSE YOUR FLEET", 2, 1, C_CYAN);
    Art_Panel(6, SHIP_ROW_Y0 - 6, GFX_W - 12, 3 * SHIP_ROW_H + 12, C_NAVY_2, C_PANEL_LN);

    if (v->screen == SCR_SHIPS_READY) {
        Gfx_TitleCenter(GFX_W / 2, 110, "READY!", 5, 2, 0x9CFF8AU, 0x2DB84AU, C_OUTLINE, 2);
        Gfx_TextCenter(GFX_W / 2, 168, "WAITING FOR", 2, 2, C_WHITE);
        Gfx_TextCenter(GFX_W / 2, 190, "OPPONENT", 2, 2, C_WHITE);
    } else {
        for (i = 1; i <= 3; ++i) {
            const int top = SHIP_ROW_Y0 + (i - 1) * SHIP_ROW_H;
            const int cy = top + SHIP_ROW_H / 2;
            const bool sel = v->selected_len == i;
            const bool none = v->remaining[i] == 0;
            const Color tc = none ? C_GREY : C_WHITE;
            if (sel) {
                /* yellow corner brackets around the selected type */
                const Color b = 0xFFD23AU;
                const int l = 28, r = 132;
                Gfx_Rect(l, top + 4, 10, 2, b);       Gfx_Rect(l, top + 4, 2, 10, b);
                Gfx_Rect(l, top + 44, 10, 2, b);      Gfx_Rect(l, top + 36, 2, 10, b);
                Gfx_Rect(r - 10, top + 4, 10, 2, b);  Gfx_Rect(r - 2, top + 4, 2, 10, b);
                Gfx_Rect(r - 10, top + 44, 10, 2, b); Gfx_Rect(r - 2, top + 36, 2, 10, b);
                Gfx_Text(12, cy - 7, "\x10", 2, 0, C_WHITE);
            }
            Art_Boat(80 - Art_BoatWidth(i, 2) / 2, cy + 14, i, 2,
                     none ? 0x3A4250U : 0x8A5A3CU, none ? 0x58606CU : 0xE8D2B0U);
            snprintf(buf, sizeof buf, "[%dx1] *%u", i, (unsigned)v->remaining[i]);
            Gfx_Text(140, cy - 7, buf, 2, 1, tc);
        }
    }
    snprintf(buf, sizeof buf, "PLACED %u / 6", (unsigned)v->placed);
    Gfx_TextCenter(GFX_W / 2, 238, buf, 2, 1, C_WHITE);

    Gfx_Rect(0, SHIP_BAR_Y, GFX_W, GFX_H - SHIP_BAR_Y, C_BLACK);
    if (v->screen == SCR_SHIPS) {
        const Color ok = v->complete ? 0xFFD23AU : C_GREY;
        Art_Panel(BTN_L_X, BTN_Y, BTN_W, BTN_H, 0x15151CU, ok);
        Gfx_TextCenter(BTN_L_X + BTN_W / 2, BTN_Y + 15, "CONFIRM", 2, 1, ok);
        Art_Panel(BTN_R_X, BTN_Y, BTN_W, BTN_H, 0x15151CU, 0xE0443AU);
        Gfx_TextCenter(BTN_R_X + BTN_W / 2, BTN_Y + 15, "CANCEL", 2, 1, 0xFF6A5AU);
    } else {
        Gfx_TextCenter(GFX_W / 2, BTN_Y + 15, "FLEET LOCKED", 2, 1, C_GREY);
    }
}

static void draw_battle(const TftView *v)
{
    char buf[28], co[8];
    const char *enemy = v->enemy_is_ai ? "AI" : "ENEMY";
    int i;
    Gfx_Rect(0, 0, GFX_W, GFX_H, C_NAVY);
    Gfx_TextCenter(GFX_W / 2, 8, "GAME STATUS", 2, 2, C_WHITE);
    title(v->starting ? "READY!" : "BATTLE!", 30, 5);

    /* last event line */
    if (v->starting) {
        snprintf(buf, sizeof buf, "%s FIRST", v->first_is_me ? "YOU FIRE" : enemy);
        Gfx_TextCenter(GFX_W / 2, 74, buf, 2, 1, C_CYAN);
    } else if (v->last_valid) {
        const char *res = v->last_result == SHOT_MISS ? "MISS"
                        : (v->last_result == SHOT_HIT ? "HIT!" : "SUNK!");
        const Color rc = v->last_result == SHOT_MISS ? C_CYAN
                       : (v->last_result == SHOT_HIT ? C_ORANGE : 0xFF4A3AU);
        coord_text(co, sizeof co, v->last_x, v->last_y);
        snprintf(buf, sizeof buf, "%s %s %s", v->last_by_me ? "YOU" : enemy, co, res);
        Gfx_TextCenter(GFX_W / 2, 74, buf, 2, 1, rc);
    } else {
        Gfx_TextCenter(GFX_W / 2, 74, "FIRE AT WILL", 2, 1, C_CYAN);
    }

    /* your fleet: 6 icons, sunk ones dark with a red X */
    Art_Panel(6, 92, 112, 128, C_NAVY_2, C_PANEL_LN);
    Gfx_TextCenter(62, 100, "YOUR SHIPS", 1, 1, C_WHITE);
    for (i = 0; i < SHIP_COUNT; ++i) {
        static const int len[SHIP_COUNT] = {3, 3, 2, 2, 1, 1};
        static const int px[SHIP_COUNT] = {12, 64, 18, 66, 22, 70};
        const int row = i / 2, w = Art_BoatWidth(len[i], 1);
        const int x = px[i] + (i % 2 ? 0 : 0), wy = 132 + row * 22;
        const bool sunk = (v->my_sunk_mask >> i) & 1U;
        Art_Boat(x, wy, len[i], 1, sunk ? 0x30343CU : 0x9AA6B4U, sunk ? 0x464B55U : 0xDDE4EAU);
        if (sunk) {
            Gfx_Line(x + 2, wy - 11, x + w - 2, wy - 1, 0xFF3A2EU);
            Gfx_Line(x + 2, wy - 1, x + w - 2, wy - 11, 0xFF3A2EU);
        }
    }
    snprintf(buf, sizeof buf, "%u/6 LEFT", (unsigned)v->my_left);
    Gfx_TextCenter(62, 198, buf, 2, 1, C_WHITE);

    /* enemy fleet */
    Art_Panel(122, 92, 112, 128, C_NAVY_2, C_PANEL_LN);
    Gfx_TextCenter(178, 100, v->enemy_is_ai ? "AI SHIPS" : "ENEMY SHIPS", 1, 1, C_WHITE);
    Art_Crosshair(178, 150, 24, C_WHITE, 0xFF3A2EU);
    snprintf(buf, sizeof buf, "%u TARGETS", (unsigned)v->enemy_left);
    Gfx_TextCenter(178, 198, buf, 2, 1, C_WHITE);

    snprintf(buf, sizeof buf, "MY SHIPS:%u", (unsigned)v->my_left);
    Gfx_Text(8, 228, buf, 2, 0, C_CYAN);
    snprintf(buf, sizeof buf, "TARGETS:%u", (unsigned)v->enemy_left);
    Gfx_Text(GFX_W - 8 - Gfx_TextWidth(buf, 2, 0), 228, buf, 2, 0, C_ORANGE);

    /* bottom bar */
    Gfx_Rect(0, 248, GFX_W, GFX_H - 248, C_BLACK);
    Gfx_Frame(4, 250, GFX_W - 8, GFX_H - 254, 2, C_MENU_LN);
    snprintf(buf, sizeof buf, "SHOTS:%u", (unsigned)v->shots);
    Gfx_Text(14, 260, buf, 2, 1, C_WHITE);
    snprintf(buf, sizeof buf, "ROUND:%u", (unsigned)v->round);
    Gfx_Text(GFX_W - 14 - Gfx_TextWidth(buf, 2, 1), 260, buf, 2, 1, C_WHITE);
    {
        const bool mine = v->my_turn && !v->starting;
        Art_Panel(12, 282, 106, 28, mine ? 0x6A3FB5U : 0x2A2E38U, mine ? 0xC9B2FFU : C_GREY);
        Gfx_TextCenter(65, 289, mine ? "\x10 FIRE!" : "WAIT", 2, 1, mine ? C_WHITE : C_GREY);
        Art_Panel(122, 282, 106, 28, v->muted ? 0x2A2E38U : 0xA82A2AU,
                  v->muted ? C_GREY : 0xFF8A80U);
        Gfx_TextCenter(175, 289, v->muted ? "MUTED" : "SOUND", 2, 1,
                       v->muted ? C_GREY : C_WHITE);
    }
}

static void stats_line(const TftView *v, int y, Color c)
{
    char buf[28];
    const unsigned acc = v->shots ? (unsigned)(v->hits * 100U / v->shots) : 0U;
    snprintf(buf, sizeof buf, "SHOTS %u  HIT %u%%", (unsigned)v->shots, acc);
    Gfx_TextCenter(GFX_W / 2, y, buf, 2, 1, c);
}

static void draw_defeat(const TftView *v)
{
    static const int stars[][2] = {{20, 30}, {60, 120}, {200, 40}, {150, 110},
                                   {220, 150}, {30, 170}, {110, 140}};
    int i;
    Gfx_VGradient(0, 0, GFX_W, 200, 0x0B1022U, 0x3A4058U, 8);
    for (i = 0; i < 7; ++i) {
        Gfx_Rect(stars[i][0], stars[i][1], 2, 2, 0xC8CCD8U);
    }
    Gfx_TitleCenter(GFX_W / 2, 20, "DEFEAT", 5, 2, 0xFF7A5AU, 0xB0120EU, 0x2A0505U, 2);
    Gfx_TextCenter(GFX_W / 2, 66, v->surrendered ? "YOU SURRENDERED" : "GAME OVER", 2, 1, 0xFF4A3AU);
    stats_line(v, 86, 0xC8CCD8U);
    Art_Sea(0, 200, GFX_W, MENU_Y - 200, 0x1A2A48U, 0x0A1426U, 0x4A6A9AU, 3);
    Art_Battleship(48, 228, 2, &SHIP_NIGHT);
    Art_Fire(110, 208, 2);
    Art_Fire(150, 214, 1);
    menu_panel("RETRY", "MAIN MENU", 1);
}

static void draw_victory(const TftView *v)
{
    Gfx_VGradient(0, 0, GFX_W, 120, 0x34186AU, 0xC2457AU, 6);
    Gfx_VGradient(0, 120, GFX_W, 80, 0xD8506EU, 0xFFA040U, 5);
    Art_Sun(184, 196, 28, 0xFFD04AU, 0xFF8A3CU);
    Art_Firework(44, 128, 24, 0xFF5A8AU);
    Art_Firework(202, 112, 18, 0x5AD1FFU);
    Art_Firework(128, 140, 14, 0xFFE14AU);
    Gfx_TitleCenter(GFX_W / 2, 22, "VICTORY!", 4, 2, C_GOLD_T, C_GOLD_B, 0x3A0F18U, 2);
    Gfx_TextCenter(GFX_W / 2, 60, v->surrendered ? "ENEMY SURRENDERED" : "YOU WIN!", 2, 1, 0xFFE14AU);
    stats_line(v, 80, C_WHITE);
    Art_Sea(0, 200, GFX_W, MENU_Y - 200, 0x6A3C8AU, 0x2A1650U, 0xFFB070U, 5);
    Art_Battleship(48, 224, 2, &SHIP_DUSK);
    menu_panel("NEXT GAME", "MAIN MENU", 1);
}

static void draw_reset(void)
{
    Gfx_Rect(0, 0, GFX_W, GFX_H, C_BLACK);
    Gfx_TextCenter(GFX_W / 2, 150, "RESETTING...", 2, 2, C_GREY);
}

void TftUi_Draw(const TftView *v)
{
    switch ((TftScreen)v->screen) {
    case SCR_HOME:
    case SCR_HOME_WAIT:   draw_home(v);      break;
    case SCR_AI:          draw_other_side(); break;
    case SCR_SHIPS:
    case SCR_SHIPS_READY: draw_ships(v);   break;
    case SCR_BATTLE:      draw_battle(v);  break;
    case SCR_DEFEAT:      draw_defeat(v);  break;
    case SCR_VICTORY:     draw_victory(v); break;
    default:              draw_reset();    break;
    }
}

/* ---------------- touch ---------------- */

static bool inside(int x, int y, int rx, int ry, int rw, int rh)
{
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

TftAction TftUi_HitTest(const Game *g, uint8_t p, int x, int y)
{
    TftAction a = {BTN_NONE, BTN_NONE};
    TftView v;
    TftUi_BuildView(g, p, &v);
    switch ((TftScreen)v.screen) {
    case SCR_HOME:
        if (y >= MENU_Y && y < MENU_LINE2) {
            a.first = BTN_MODE_1P;
            a.second = BTN_CONFIRM;
        } else if (y >= MENU_LINE2) {
            a.first = BTN_MODE_2P;
            a.second = BTN_CONFIRM;
        }
        break;
    case SCR_SHIPS:
        if (y >= SHIP_ROW_Y0 && y < SHIP_ROW_Y0 + 3 * SHIP_ROW_H) {
            a.first = (GameButton)(BTN_SHIP_1 + (y - SHIP_ROW_Y0) / SHIP_ROW_H);
        } else if (inside(x, y, BTN_L_X, BTN_Y, BTN_W, BTN_H)) {
            a.first = BTN_CONFIRM;
        } else if (inside(x, y, BTN_R_X, BTN_Y, BTN_W, BTN_H)) {
            a.first = BTN_CANCEL;
        }
        break;
    case SCR_DEFEAT:
    case SCR_VICTORY:
        if (y >= MENU_Y && y < MENU_LINE2) {
            a.first = BTN_REMATCH;
        } else if (y >= MENU_LINE2) {
            a.first = BTN_MAIN_MENU;
        }
        break;
    default:
        break;
    }
    return a;
}

/* Bands of gfx_fb that differ from what display p shows; updates the record. */
static uint32_t changed_bands(uint8_t p)
{
    uint32_t mask = 0;
    int b;
    for (b = 0; b < GFX_BANDS; ++b) {
        const uint32_t h = Gfx_BandHash(b);
        if (!hash_valid[p] || h != band_hash[p][b]) {
            band_hash[p][b] = h;
            mask |= 1UL << b;
        }
    }
    hash_valid[p] = true;
    return mask;
}

int TftUi_Render(const Game *g)
{
    uint8_t k;
    if (Tft_IsBusy()) {
        return 0;               /* gfx_fb is being sent: draw nothing yet */
    }
    /* At most one display per call, alternating so neither starves. */
    for (k = 0; k < GAME_PLAYERS; ++k) {
        const uint8_t p = (uint8_t)((next_player + k) % GAME_PLAYERS);
        uint32_t mask;
        TftView v;
        TftUi_BuildView(g, p, &v);
        if (shown_valid[p] && memcmp(&v, &shown[p], sizeof v) == 0) {
            continue;
        }
        TftUi_Draw(&v);                 /* whole frame into RAM, a few ms */
        mask = changed_bands(p);        /* usually only a few bands */
        shown[p] = v;
        shown_valid[p] = true;
        next_player = (uint8_t)((p + 1U) % GAME_PLAYERS);
        if (mask != 0U) {
            last_mask = mask;
            Tft_SendBands(p, mask);     /* DMA, returns immediately */
            return 1;
        }
    }
    return 0;
}

uint32_t TftUi_LastBandMask(void)
{
    return last_mask;
}
