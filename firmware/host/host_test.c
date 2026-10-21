/* Host tests for the HAL-free firmware modules. Build: run_host_tests.ps1 */
#include "buzzer.h"
#include "game.h"
#include "keypad.h"
#include "lcd_ui.h"
#include "led_view.h"
#include "gfx.h"
#include "tft_ui.h"
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

extern unsigned host_tft_blits;

static int popcount(uint32_t m)
{
    int n = 0;
    for (; m != 0U; m &= m - 1U) {
        ++n;
    }
    return n;
}

static void test_touch_screens(void)
{
    Game g;
    TftView v;
    TftAction a;
    unsigned before;
    memset(&g, 0, sizeof g);
    Game_Init(&g, 99, AI_BALANCED);
    TftUi_Init();

    /* S1: P1 home, P2 waits; tapping menu line 2 = choose 2P and start */
    TftUi_BuildView(&g, 0, &v);
    assert(v.screen == SCR_HOME);
    TftUi_BuildView(&g, 1, &v);
    assert(v.screen == SCR_HOME_WAIT);
    assert(TftUi_HitTest(&g, 1, 120, 300).first == BTN_NONE);
    a = TftUi_HitTest(&g, 0, 120, 300);
    assert(a.first == BTN_MODE_2P && a.second == BTN_CONFIRM);
    Game_OnButton(&g, 0, a.first);
    Game_OnButton(&g, 0, a.second);
    assert(g.state == GS_PLACE_SHIPS && g.mode == MODE_2P);

    /* S2: real remaining counts, rows select ship types */
    a = TftUi_HitTest(&g, 0, 120, 70 + 2 * 50 + 10);   /* third row = 3x1 */
    assert(a.first == BTN_SHIP_3);
    Game_OnButton(&g, 0, a.first);
    Game_OnKey(&g, PANEL_P1_FLEET, 0, 0);
    Game_OnKey(&g, PANEL_P1_FLEET, 1, 0);
    Game_OnKey(&g, PANEL_P1_FLEET, 2, 0);
    TftUi_BuildView(&g, 0, &v);
    assert(v.screen == SCR_SHIPS && v.remaining[3] == 1 && v.remaining[2] == 2 &&
           v.placed == 1 && v.selected_len == 3 && !v.complete);
    assert(TftUi_HitTest(&g, 0, 60, 290).first == BTN_CONFIRM);
    assert(TftUi_HitTest(&g, 0, 180, 290).first == BTN_CANCEL);

    /* render: one display per call, first frame = all 20 bands */
    before = host_tft_blits;
    assert(TftUi_Render(&g) == 1 && TftUi_LastBandMask() == 0xFFFFFU);
    assert(TftUi_Render(&g) == 1 && TftUi_LastBandMask() == 0xFFFFFU);
    assert(TftUi_Render(&g) == 0 && host_tft_blits == before + 2);
    Game_OnKey(&g, PANEL_P1_FLEET, 5, 5);   /* pending cell: view unchanged */
    assert(TftUi_Render(&g) == 0);
    Game_OnButton(&g, 0, BTN_CANCEL);       /* only P1 changes: ship rows + count */
    assert(TftUi_Render(&g) == 1 && TftUi_Render(&g) == 0);
    printf("  ship list change sends %d/20 bands\n", popcount(TftUi_LastBandMask()));
    assert(popcount(TftUi_LastBandMask()) < GFX_BANDS);

    place_fleet(&g, 0);
    Game_OnButton(&g, 0, BTN_CONFIRM);
    TftUi_BuildView(&g, 0, &v);
    assert(v.screen == SCR_SHIPS_READY);
    place_fleet(&g, 1);
    Game_OnButton(&g, 1, BTN_CONFIRM);
    tick(&g, 2000);

    /* S4: stats follow the match */
    {
        const uint8_t p = g.match.current_player;
        Game_OnKey(&g, GAME_ATTACK_PANEL(p), 0, 0);   /* hit */
        Game_OnKey(&g, GAME_ATTACK_PANEL(p), 1, 0);   /* hit */
        Game_OnKey(&g, GAME_ATTACK_PANEL(p), 2, 0);   /* sunk (3-long) */
        Game_OnKey(&g, GAME_ATTACK_PANEL(p), 6, 4);   /* miss: turn passes */
        TftUi_BuildView(&g, p, &v);
        assert(v.screen == SCR_BATTLE && v.shots == 4 && v.hits == 3 &&
               v.enemy_left == 5 && v.my_left == 6 && !v.my_turn && v.round == 1);
        TftUi_BuildView(&g, (uint8_t)(1U - p), &v);
        assert(v.my_left == 5 && v.my_sunk_mask == 0x01 && v.my_turn &&
               v.last_valid && !v.last_by_me && v.last_result == SHOT_MISS);
        Game_OnKey(&g, GAME_ATTACK_PANEL(1U - p), 6, 4); /* miss: back to p */
        assert(g.round == 2);

        /* one more shot after both battle screens are up: only the bands
         * with changed numbers/text are sent */
        while (TftUi_Render(&g) != 0) { }
        Game_OnKey(&g, GAME_ATTACK_PANEL(p), 7, 6);       /* miss */
        assert(TftUi_Render(&g) == 1);
        printf("  battle shot sends %d/20 bands (first display)\n",
               popcount(TftUi_LastBandMask()));
        assert(popcount(TftUi_LastBandMask()) <= 8);
        assert(TftUi_Render(&g) == 1 && popcount(TftUi_LastBandMask()) <= 8);
        assert(TftUi_Render(&g) == 0);

        /* S5: defeat/victory screens, RETRY keeps the mode */
        Game_OnButton(&g, p, BTN_SURRENDER);
        TftUi_BuildView(&g, p, &v);
        assert(v.screen == SCR_DEFEAT && v.surrendered);
        TftUi_BuildView(&g, (uint8_t)(1U - p), &v);
        assert(v.screen == SCR_VICTORY);
        a = TftUi_HitTest(&g, p, 120, 260);
        assert(a.first == BTN_REMATCH);
        Game_OnButton(&g, p, a.first);
        assert(g.state == GS_PLACE_SHIPS && g.mode == MODE_2P && g.placing[0].placed == 0);
    }

    /* MAIN MENU path */
    place_fleet(&g, 0);
    Game_OnButton(&g, 0, BTN_CONFIRM);
    place_fleet(&g, 1);
    Game_OnButton(&g, 1, BTN_CONFIRM);
    Game_OnButton(&g, 0, BTN_SURRENDER);
    Game_OnButton(&g, 1, TftUi_HitTest(&g, 1, 120, 300).first);
    assert(g.state == GS_RESET);
    tick(&g, 1000);
    assert(g.state == GS_SELECT_MODE);
    puts("touch screens (views, hit-test, redraw, rematch): OK");
}

extern char host_lcd[2][2][17];
extern unsigned host_lcd_writes;

static void expect_lcd(const Game *g, uint8_t p, const char *a, const char *b)
{
    char l0[LCD_UI_COLS + 1], l1[LCD_UI_COLS + 1];
    LcdUi_Format(g, p, l0, l1);
    if (strcmp(l0, a) != 0 || strcmp(l1, b) != 0) {
        printf("LCD P%u got [%s|%s] want [%s|%s]\n", (unsigned)p + 1U, l0, l1, a, b);
        assert(0);
    }
    printf("  P%u |%-16s|%-16s|\n", (unsigned)p + 1U, l0, l1);
}

static void test_lcd_pages(void)
{
    Game g;
    unsigned w;
    uint8_t f, o;
    memset(&g, 0, sizeof g);
    Game_Init(&g, 5, AI_BALANCED);
    LcdUi_Init();
    puts("LCD pages:");
    expect_lcd(&g, 0, "SELECT MODE", "> 1 PLAYER");
    expect_lcd(&g, 1, "SELECT MODE", "WAIT FOR P1");

    /* first render writes all 4 lines, an unchanged state writes none */
    w = host_lcd_writes;
    assert(LcdUi_Render(&g) == 4 && host_lcd_writes == w + 4);
    assert(strcmp(host_lcd[0][1], "> 1 PLAYER      ") == 0);
    assert(LcdUi_Render(&g) == 0);
    Game_OnButton(&g, 0, BTN_MODE_2P);
    assert(LcdUi_Render(&g) == 1);            /* only P1 line 2 changed */
    expect_lcd(&g, 0, "SELECT MODE", "> 2 PLAYERS");

    Game_OnButton(&g, 0, BTN_CONFIRM);
    Game_OnButton(&g, 0, BTN_SHIP_3);
    Game_OnKey(&g, PANEL_P1_FLEET, 0, 0);
    Game_OnKey(&g, PANEL_P1_FLEET, 1, 0);
    Game_OnKey(&g, PANEL_P1_FLEET, 2, 0);
    expect_lcd(&g, 0, "PLACE SHIPS 1/6", "L3:1 L2:2 L1:2");
    Game_OnButton(&g, 0, BTN_CANCEL);
    place_fleet(&g, 0);
    Game_OnButton(&g, 0, BTN_CONFIRM);
    expect_lcd(&g, 0, "PLACE SHIPS 6/6", "READY WAIT P2");
    place_fleet(&g, 1);
    Game_OnButton(&g, 1, BTN_CONFIRM);

    f = g.first_player;
    o = (uint8_t)(1U - f);
    expect_lcd(&g, f, "GAME START", "YOU FIRST");
    expect_lcd(&g, o, "GAME START", "ENEMY FIRST");
    tick(&g, 2000);
    expect_lcd(&g, f, "YOUR TURN     R1", "SHIPS 6 VS 6");
    expect_lcd(&g, o, "ENEMY TURN    R1", "SHIPS 6 VS 6");
    Game_OnKey(&g, GAME_ATTACK_PANEL(f), 2, 0);   /* C1: hit */
    expect_lcd(&g, f, "YOUR TURN     R1", "YOU C1 HIT");
    expect_lcd(&g, o, "ENEMY TURN    R1", "ENEMY C1 HIT");
    Game_OnKey(&g, GAME_ATTACK_PANEL(f), 7, 6);   /* H7: miss, turn passes */
    expect_lcd(&g, o, "YOUR TURN     R1", "ENEMY H7 MISS");
    Game_OnKey(&g, GAME_ATTACK_PANEL(o), 6, 4);   /* G5: miss, back -> round 2 */
    expect_lcd(&g, f, "YOUR TURN     R2", "ENEMY G5 MISS");

    /* S5: stats line, and the surrender wording */
    Game_OnKey(&g, GAME_ATTACK_PANEL(f), 1, 0);
    Game_OnKey(&g, GAME_ATTACK_PANEL(f), 0, 0);   /* sinks the 3-long ship */
    Game_OnButton(&g, o, BTN_SURRENDER);
    expect_lcd(&g, f, "YOU WIN!", "ENEMY GAVE UP");
    expect_lcd(&g, o, "YOU LOSE", "YOU GAVE UP");
    g.surrendered = false;                         /* same match, normal finish */
    expect_lcd(&g, f, "YOU WIN!", "SHOT 4 HIT 75%");
    Game_OnButton(&g, f, BTN_MAIN_MENU);
    expect_lcd(&g, 0, "RESETTING...", "");

    /* single player: the AI side shows a fixed message */
    tick(&g, 1000);
    Game_OnButton(&g, 0, BTN_MODE_1P);
    Game_OnButton(&g, 0, BTN_CONFIRM);
    expect_lcd(&g, 1, "1 PLAYER MODE", "AI SIDE");
    place_fleet(&g, 0);
    Game_OnButton(&g, 0, BTN_CONFIRM);
    expect_lcd(&g, 0, "GAME START", g.first_player == 0 ? "YOU FIRST" : "AI FIRST");
    puts("LCD pages (text per state, changed-lines-only writes): OK");
}

/* Both players place at the same time: presses arrive interleaved, one
 * player's CANCEL must not touch the other, and confirm order is free. */
static void test_simultaneous_placement(void)
{
    Game g;
    KeyEvent e;
    uint8_t ship[2] = {0, 0}, cell[2] = {0, 0}, i, p;
    bool busy = true;
    memset(&g, 0, sizeof g);
    Game_Init(&g, 11, AI_BALANCED);
    Game_OnButton(&g, 0, BTN_MODE_2P);
    Game_OnButton(&g, 0, BTN_CONFIRM);

    /* both pick a type, both start a ship, then P2 cancels mid-way */
    Game_OnButton(&g, 0, BTN_SHIP_3);
    Game_OnButton(&g, 1, BTN_SHIP_2);
    Game_OnKey(&g, PANEL_P1_FLEET, 0, 0);
    Game_OnKey(&g, PANEL_P2_FLEET, 3, 3);
    Game_OnKey(&g, PANEL_P2_FLEET, 4, 3);       /* P2 finishes a 2-long ship */
    Game_OnKey(&g, PANEL_P1_FLEET, 1, 0);
    assert(g.placing[1].placed == 1 && g.placing[0].pending_count == 2);
    Game_OnButton(&g, 1, BTN_CANCEL);
    assert(g.placing[1].placed == 0 && g.player[1].own_board[3][3] == 0);
    assert(g.placing[0].pending_count == 2 && g.placing[0].selected_length == 3);
    Game_OnButton(&g, 0, BTN_CANCEL);

    /* full fleets, one press from each side in turn */
    while (busy) {
        busy = false;
        for (p = 0; p < 2U; ++p) {
            /* P2 places its fleet in reverse order to differ from P1 */
            const uint8_t s = (uint8_t)(p == 0 ? ship[p] : 5U - ship[p]);
            if (ship[p] >= 6U) {
                continue;
            }
            busy = true;
            if (cell[p] == 0U) {
                Game_OnButton(&g, p, (GameButton)(BTN_SHIP_1 + fleet[s].len - 1));
            }
            Game_OnKey(&g, GAME_FLEET_PANEL(p), fleet[s].cells[cell[p]][0],
                       fleet[s].cells[cell[p]][1]);
            if (++cell[p] == fleet[s].len) {
                cell[p] = 0;
                ++ship[p];
            }
        }
    }
    assert(Placement_IsComplete(&g.placing[0]) && Placement_IsComplete(&g.placing[1]));
    Game_OnButton(&g, 1, BTN_CONFIRM);           /* P2 confirms first */
    assert(g.state == GS_PLACE_SHIPS && g.ready[1] && !g.ready[0]);
    Game_OnKey(&g, PANEL_P2_FLEET, 5, 5);        /* locked after confirm */
    assert(g.player[1].own_board[5][5] == 0);
    Game_OnButton(&g, 0, BTN_CONFIRM);
    assert(g.state == GS_GAME_START);

    /* switch matrix: both players press in the same scan -> two events */
    Keypad_ResetCore();
    for (i = 0; i < KEYPAD_DEBOUNCE_SCANS; ++i) {
        Keypad_ProcessColumn(2, (UINT32_C(1) << 0) | (UINT32_C(1) << 15)); /* P1 C1, P2 C2 */
    }
    assert(Keypad_GetEvent(&e) && e.panel == PANEL_P1_FLEET && e.x == 2 && e.y == 0);
    assert(Keypad_GetEvent(&e) && e.panel == PANEL_P2_FLEET && e.x == 2 && e.y == 1);
    assert(!Keypad_GetEvent(&e));
    puts("simultaneous placement (interleaved presses, independent CANCEL): OK");
}

int main(void)
{
    test_placement_rules();
    test_keypad();
    test_ws2812_encode();
    test_pve_game();
    test_pvp_surrender_and_repeat_shot();
    test_simultaneous_placement();
    test_touch_screens();
    test_lcd_pages();
    puts("ALL HOST TESTS PASSED");
    return 0;
}
