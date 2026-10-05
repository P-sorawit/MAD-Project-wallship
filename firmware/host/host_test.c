/* Host tests for the HAL-free firmware modules. Build: run_host_tests.ps1 */
#include "buzzer.h"
#include "game.h"
#include "keypad.h"
#include "led_view.h"
#include "ws2812.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

extern int host_last_sound;

static uint32_t now;

static void tick(Game *g, uint32_t ms)
{
    uint32_t i;
    for (i = 0; i < ms; i += 10) {
        now += 10;
        Game_Tick(g, now);
    }
}

static void show(const Game *g, const char *label)
{
    LedView_Render(g);
    printf("  %-4s state=S%d turn=P%u ships P1:%u P2:%u\n", label, (int)g->state,
           (unsigned)g->match.current_player + 1U,
           (unsigned)Game_ShipsLeft(g, 0), (unsigned)Game_ShipsLeft(g, 1));
}

typedef struct { uint8_t len; uint8_t cells[3][2]; } ShipSpec;

/* Includes a vertical ship entered bottom-up to exercise reverse extension. */
static const ShipSpec fleet[6] = {
    {3, {{0, 0}, {1, 0}, {2, 0}}},
    {3, {{7, 2}, {7, 1}, {7, 0}}},
    {2, {{1, 2}, {0, 2}}},
    {2, {{4, 4}, {4, 5}}},
    {1, {{2, 6}}},
    {1, {{6, 6}}},
};

static void place_fleet(Game *g, uint8_t p)
{
    uint8_t s, c;
    for (s = 0; s < 6; ++s) {
        Game_OnButton(g, p, (GameButton)(BTN_SHIP_1 + fleet[s].len - 1));
        for (c = 0; c < fleet[s].len; ++c) {
            Game_OnKey(g, GAME_FLEET_PANEL(p), fleet[s].cells[c][0], fleet[s].cells[c][1]);
        }
    }
    assert(Placement_IsComplete(&g->placing[p]));
    assert(WallshipRules_FleetReady(&g->player[p]));
}

static void test_placement_rules(void)
{
    WallshipAgent a;
    Placement pl;
    memset(&a, 0, sizeof a);
    Placement_Clear(&pl, &a);
    assert(Placement_PressCell(&pl, &a, 0, 0) == PLACE_NO_LENGTH);
    assert(Placement_SelectLength(&pl, 3));
    assert(Placement_PressCell(&pl, &a, 3, 3) == PLACE_PENDING);
    assert(Placement_PressCell(&pl, &a, 5, 3) == PLACE_REJECT);   /* gap */
    assert(Placement_PressCell(&pl, &a, 4, 4) == PLACE_REJECT);   /* diagonal */
    assert(Placement_PressCell(&pl, &a, 4, 3) == PLACE_PENDING);  /* locks horizontal */
    assert(Placement_PressCell(&pl, &a, 4, 4) == PLACE_REJECT);   /* L-shape */
    assert(Placement_PressCell(&pl, &a, 2, 3) == PLACE_SHIP_DONE);/* extend left */
    assert(a.own_board[3][2] && a.own_board[3][3] && a.own_board[3][4]);
    assert(Placement_SelectLength(&pl, 3));
    assert(Placement_PressCell(&pl, &a, 3, 3) == PLACE_REJECT);   /* overlap */
    assert(Placement_PressCell(&pl, &a, 3, 4) == PLACE_PENDING);  /* touching is ok */
    Placement_Clear(&pl, &a);                                     /* CANCEL */
    assert(pl.placed == 0 && a.own_board[3][3] == 0 && pl.remaining[3] == 2);
    puts("placement rules: OK");
}

static void test_keypad(void)
{
    KeyEvent e;
    uint8_t i;
    Keypad_ResetCore();
    /* bounce: pressed, released, pressed... never stable -> no event */
    for (i = 0; i < 10; ++i) {
        Keypad_ProcessColumn(3, (i & 1U) ? 0 : (UINT32_C(1) << 9));
    }
    assert(!Keypad_GetEvent(&e));
    for (i = 0; i < KEYPAD_DEBOUNCE_SCANS; ++i) {
        Keypad_ProcessColumn(3, UINT32_C(1) << 9); /* row 9 = panel 1, y 2 */
    }
    assert(Keypad_GetEvent(&e));
    assert(e.panel == 1 && e.x == 3 && e.y == 2);
    assert(!Keypad_GetEvent(&e));                  /* holding: no repeat */
    for (i = 0; i < 10; ++i) {
        Keypad_ProcessColumn(3, UINT32_C(1) << 9);
    }
    assert(!Keypad_GetEvent(&e));
    puts("keypad debounce/mapping: OK");
}

static void test_ws2812_encode(void)
{
    const Rgb red = {255, 0, 0};
    uint8_t i;
    Ws2812_Init();
    Ws2812_SetPixel(0, 0, 0, red);
    Ws2812_Encode();
    /* G = 0 -> eight CCR_0; R = 255 * 40% = 102 = 0b01100110 */
    for (i = 0; i < 8; ++i) {
        assert(ws2812_dma[0][i] == WS2812_CCR_0);
    }
    {
        static const uint16_t r[8] = {86, 173, 173, 86, 86, 173, 173, 86};
        for (i = 0; i < 8; ++i) {
            assert(ws2812_dma[0][8 + i] == r[i]);
        }
    }
    assert(ws2812_dma[0][WS2812_DMA_LEN - 1] == 0);  /* reset slot */
    assert(ws2812_dma[1][8] == WS2812_CCR_0);        /* other panel untouched */
    assert(Ws2812_Show() && !Ws2812_IsBusy());
    puts("ws2812 encode: OK");
}

static void test_pve_game(void)
{
    Game g;
    uint8_t x = 0, y = 0;
    unsigned shots = 0;
    memset(&g, 0, sizeof g);
    now = 1000;
    g.now_ms = now;
    Game_Init(&g, 123, AI_BALANCED);
    puts("PvE game:");
    show(&g, "S1");
    Game_OnButton(&g, 1, BTN_MODE_2P);  /* P2 cannot choose the mode */
    assert(g.mode_cursor == MODE_1P);
    Game_OnButton(&g, 0, BTN_MODE_1P);
    Game_OnButton(&g, 0, BTN_CONFIRM);  /* START */
    assert(g.state == GS_PLACE_SHIPS && g.mode == MODE_1P && g.ready[1]);
    Game_OnButton(&g, 0, BTN_CONFIRM);  /* fleet incomplete: rejected */
    assert(g.state == GS_PLACE_SHIPS && !g.ready[0] && host_last_sound == SND_ERROR);
    show(&g, "S2");
    place_fleet(&g, 0);
    Game_OnButton(&g, 0, BTN_CONFIRM);
    assert(g.state == GS_GAME_START);
    show(&g, "S3");
    tick(&g, 2000);
    assert(g.state == GS_BATTLE);
    while (g.state == GS_BATTLE) {
        if (g.match.current_player == 0) {
            Game_OnKey(&g, PANEL_P1_ATTACK, x, y);
            ++shots;
            if (++x == BOARD_W) { x = 0; ++y; }
        } else {
            tick(&g, 1000);
        }
        if (shots == 5) {
            show(&g, "S4");
        }
    }
    show(&g, "S5");
    assert(g.state == GS_RESULT && g.winner <= 1);
    printf("  winner: %s after %u human shots\n", g.winner == 0 ? "human" : "AI", shots);
    /* S5 reveals: AI's unshot ships are white on P1's attack panel. */
    Game_OnButton(&g, 0, BTN_CONFIRM);
    assert(g.state == GS_RESET);
    show(&g, "S6");
    tick(&g, 1000);
    assert(g.state == GS_SELECT_MODE);
    puts("PvE full game S1->S6->S1: OK");
}

static void test_pvp_surrender_and_repeat_shot(void)
{
    Game g;
    memset(&g, 0, sizeof g);
    Game_Init(&g, 7, AI_BALANCED);
    Game_OnButton(&g, 0, BTN_MODE_2P);
    Game_OnButton(&g, 0, BTN_CONFIRM);
    assert(g.mode == MODE_2P && !g.ready[1]);
    place_fleet(&g, 0);
    Game_OnButton(&g, 0, BTN_CONFIRM);
    assert(g.state == GS_PLACE_SHIPS); /* waiting for P2 */
    place_fleet(&g, 1);
    Game_OnButton(&g, 1, BTN_CONFIRM);
    assert(g.state == GS_GAME_START);
    tick(&g, 2000);
    {
        const uint8_t p = g.match.current_player;
        Game_OnKey(&g, GAME_ATTACK_PANEL(1U - p), 5, 5); /* not your turn: ignored */
        assert(!g.has_last_shot);
        Game_OnKey(&g, GAME_ATTACK_PANEL(p), 0, 0);      /* hit -> keep turn */
        assert(g.last_result == SHOT_HIT && g.match.current_player == p);
        Game_OnKey(&g, GAME_ATTACK_PANEL(p), 0, 0);      /* repeat -> error beep */
        assert(host_last_sound == SND_ERROR && g.match.current_player == p);
        Game_OnButton(&g, p, BTN_SURRENDER);
        assert(g.state == GS_RESULT && g.winner == 1U - p && g.surrendered);
    }
    puts("PvP turn/repeat/surrender: OK");
}

int main(void)
{
    test_placement_rules();
    test_keypad();
    test_ws2812_encode();
    test_pve_game();
    test_pvp_surrender_and_repeat_shot();
    puts("ALL HOST TESTS PASSED");
    return 0;
}
