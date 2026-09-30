# Wallship heuristic AI

คำอธิบาย heuristic แบบหน้าเว็บพร้อม Score Map ทดลองได้อยู่ที่ [heuristic_explained.html](heuristic_explained.html) และวิธีเล่น demo อยู่ที่ [PLAY.md](PLAY.md)

## 1. Architecture overview

The core is standard C99 with no HAL dependencies, allocation, floating point, recursion, OS calls, machine learning, or `rand()`. All boards use `[y][x]`, with `(0,0)` at the top left and `(7,6)` at the bottom right. Each side has two ships each of lengths 1, 2, and 3. Ships may touch but cannot overlap.

The core is header-only: `wallship_types.h` holds shared types, `wallship_heuristic.h` computes scores and provides the RNG, and `wallship_ai.h` places ships, selects shots, and accepts feedback. Include only `wallship_ai.h` for the complete AI API; it includes the other headers. `test_wallship_ai.c` is the desktop assertion suite. Additional integration files are `examples/wallship_game.h` and `examples/wallship_game.c`, with host tests in `test_wallship_game.c`. `test_placement_failure.c` exercises a forced failure build.

`wallship_game_rules.h` is a separate header-only rule library for two players. It previews a shot, applies hit/miss/sunk damage, checks whether a fleet has sunk, and manages turn and winner state. The compatibility functions `WallshipAI_ProcessHumanShot` and `WallshipAI_ShouldContinueTurn` in `wallship_ai.h` delegate to these rules.

For a two-player game, set up a `WallshipAgent` with a complete fleet for each player, then use:

```c
#include "wallship_game_rules.h"

WallshipTwoPlayerGame game;
uint8_t sunk_length;
if (WallshipTwoPlayerGame_Init(&game, &player0, &player1, 0)) {
    ShotResult result = WallshipTwoPlayerGame_Shoot(&game, game.current_player,
                                                    x, y, &sunk_length);
    /* Handle result here. */
}
/* game.current_player is the next shooter; game.finished and game.winner
 * report the outcome. Invalid shots do not advance the turn. */
```

`WallshipRules_PreviewShot(&player1, x, y)` checks the result without changing state. `WallshipTwoPlayerGame_Init` accepts only distinct players with complete unshot fleets. Include `wallship_game_rules.h` alone for rules, or `wallship_ai.h` when you also need fleet placement or AI functions. Call these state-changing functions from the game loop, rather than concurrently from an interrupt.

For STM32 code that only needs scoring, include `wallship_heuristic.h` and call `WallshipAI_CalculateScores(&ai, score)` after setting the enemy board, remaining ship counts, profile, and RNG seed. No AI `.c` file needs to be compiled. For the full AI, include `wallship_ai.h`; the board application still compiles its own transport and main loop.

For an interactive desktop game against these profiles, see [PLAY.md](PLAY.md). Its gameplay loop lives separately in `wallship_play.c` and uses the same AI core.

Initialize, explicitly place ships and check success, then select a shot, send it once, and report its result. Only one shot may be pending. Selection while waiting returns `{255,255}` without consuming RNG state. A selected cell remains UNKNOWN until feedback, but the pending flag prevents another selection. Invalid or mismatched feedback leaves the transaction pending for correction. No API cancels a transmitted shot and makes it selectable again.

SUNK is a two-stage operation: first report SHOT_SUNK at the attacked coordinate, then supply the entire ship's coordinates. The core records the final segment as HIT until a complete valid SUNK event can be committed. The integration layer buffers details that arrive before the result. A NewGame resets state while preserving the profile; neither Init nor NewGame places ships automatically. All core calls run in the main loop.

Run the tests on the provided Windows environment:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run_tests.ps1
```

This sets execution policy only for that PowerShell process. The runner compiles with GCC C99, optimization, strict warnings as errors, and stack-usage reports. It checks each public header independently and links integration tests without an AI object file. It stops at the first compilation error or assertion failure. Output goes into `build/`. Keep assertions enabled; do not compile tests with `-DNDEBUG`.

Equivalent portable GCC commands (run each resulting executable):

```sh
gcc -std=c99 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -Werror test_wallship_ai.c -o test_wallship_ai
gcc -std=c99 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -Werror test_header_scoring.c -o test_header_scoring
gcc -std=c99 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -Werror test_game_rules.c -o test_game_rules
gcc -std=c99 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -Werror -DMAX_SHIP_PLACEMENT_ATTEMPTS=0 test_placement_failure.c -o test_placement_failure
gcc -std=c99 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -Werror -I. examples/wallship_game.c test_wallship_game.c -o test_wallship_game
```

Validated with MinGW GCC 6.3.0, `mingw32`, `-O2`: 1,000 placement seeds, 3,000 attack simulations (1,000 per profile), 60 full two-player integration games, forced placement failure, and focused boundary, protocol, targeting, counter, and interrupt-mailbox tests. Every attack simulation won within 56 unique shots. Mean shots were 52.479 random, 42.427 balanced, and 42.724 aggressive. These deterministic sample results do not imply aggressive is always better than balanced. No STM32 compiler or hardware was available; hardware timing and transport behavior remain to be measured in the application.

## 2. Header-only libraries

| Header | Purpose | Direct dependencies |
| --- | --- | --- |
| `wallship_types.h` | Board constants, cell/shot enums, ship and agent state | Standard C headers |
| `wallship_heuristic.h` | Score profiles, RNG, placement-density scoring | `wallship_types.h` |
| `wallship_game_rules.h` | Shot rules, turns, and two-player winner state | `wallship_types.h` |
| `wallship_ai.h` | Fleet placement, shot selection, feedback, legacy rule wrappers | The three headers above |

Each header can be included on its own. The functions use `static inline`, so there is no AI object file to link. Code that uses only scoring includes `wallship_heuristic.h`; code that uses the complete AI includes `wallship_ai.h`. Keep the headers in the compiler include path, and compile the board application's own `.c` files as usual.

## 3. AI API and STM32 ownership

`WallshipAI_Init` sets the profile and seed; `WallshipAI_PlaceShips` must then succeed before play. `WallshipAI_SelectShot` reserves one coordinate until `WallshipAI_ReportShotResult` accepts its result. After `SHOT_SUNK`, call `WallshipAI_ReportSunkShip` with every cell of that ship. `WallshipAI_ProcessHumanShot` and `WallshipAI_ShouldContinueTurn` retain their old names and delegate to `wallship_game_rules.h`.

The example in section 5 supplies the event mailbox and main-loop state machine for STM32 integration. Its `examples/wallship_game.c` remains a separate application file; the AI logic itself is header-only. Use board-specific transport, display, and IRQ hooks from that example, and keep game API calls in main context.

## 4. test_wallship_ai.c

Full source (the failure-build and integration tests are separate companion files):

```c
#include "wallship_ai.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static bool inside(Position p)
{
    return p.x < BOARD_W && p.y < BOARD_H;
}

static void assert_invalid(Position p)
{
    assert(p.x == INVALID_COORD && p.y == INVALID_COORD);
}

static void fill_enemy(WallshipAgent *ai, CellState state)
{
    int x, y;
    for (y = 0; y < BOARD_H; ++y) {
        for (x = 0; x < BOARD_W; ++x) {
            ai->enemy_board[y][x] = state;
        }
    }
}

/* Fixtures may set public state to isolate a pending transaction. Gameplay
 * simulations below exclusively select shots through the public API.
 */
static void pending_sunk(WallshipAgent *ai, uint8_t x, uint8_t y)
{
    ai->last_shot.x = x;
    ai->last_shot.y = y;
    ai->last_shot_valid = true;
    ai->awaiting_sunk_details = false;
    ai->enemy_board[y][x] = CELL_UNKNOWN;
    WallshipAI_ReportShotResult(ai, x, y, SHOT_SUNK);
    assert(ai->awaiting_sunk_details && ai->last_shot_valid);
    assert(ai->enemy_board[y][x] == CELL_HIT);
}

static unsigned verify_fleet(const WallshipAgent *ai)
{
    static const uint8_t lengths[SHIP_COUNT] = {3, 3, 2, 2, 1, 1};
    uint8_t expected[BOARD_H][BOARD_W] = {{0}};
    unsigned occupied = 0, touching = 0;
    int x, y, i, j;
    for (i = 0; i < SHIP_COUNT; ++i) {
        const Ship *s = &ai->ships[i];
        assert(s->id == i+1 && s->length == lengths[i]);
        assert(s->hp == s->length && !s->sunk);
        assert(s->orientation == SHIP_HORIZONTAL || s->orientation == SHIP_VERTICAL);
        for (j = 0; j < s->length; ++j) {
            x = s->start_x + (s->orientation == SHIP_HORIZONTAL ? j : 0);
            y = s->start_y + (s->orientation == SHIP_VERTICAL ? j : 0);
            assert(x >= 0 && x < BOARD_W && y >= 0 && y < BOARD_H);
            assert(expected[y][x] == 0);
            expected[y][x] = s->id;
        }
    }
    assert(memcmp(expected, ai->own_board, sizeof expected) == 0);
    for (y = 0; y < BOARD_H; ++y) {
        for (x = 0; x < BOARD_W; ++x) {
            const uint8_t id = ai->own_board[y][x];
            assert(ai->human_shot_board[y][x] == 0);
            if (id != 0) {
                ++occupied;
                if (x+1 < BOARD_W && ai->own_board[y][x+1] != 0 &&
                    ai->own_board[y][x+1] != id) { ++touching; }
                if (y+1 < BOARD_H && ai->own_board[y+1][x] != 0 &&
                    ai->own_board[y+1][x] != id) { ++touching; }
            }
        }
    }
    assert(occupied == 12);
    return touching;
}

static void test_initialization_and_placement(void)
{
    WallshipAgent a, b;
    uint8_t first_board[BOARD_H][BOARD_W];
    uint32_t seed;
    unsigned varied = 0, touching = 0;
    int i;
    assert(BOARD_W == 8 && BOARD_H == 7 && SHIP_COUNT == 6);
    WallshipAI_Init(&a, 0, (AIProfile)99);
    assert(a.profile == AI_BALANCED && a.rng_state == WALLSHIP_DEFAULT_SEED);
    assert(a.remaining_ship_1 == 2 && a.remaining_ship_2 == 2 && a.remaining_ship_3 == 2);
    assert(!a.last_shot_valid && !a.awaiting_sunk_details);
    assert_invalid(a.last_shot);
    for (i = 0; i < SHIP_COUNT; ++i) { assert(a.ships[i].hp == 0); }
    assert(WallshipAI_PlaceShips(&a));
    memcpy(first_board, a.own_board, sizeof first_board);
    for (seed = 1; seed <= 1000; ++seed) {
        WallshipAI_Init(&a, seed, AI_BALANCED);
        WallshipAI_Init(&b, seed, AI_BALANCED);
        assert(WallshipAI_PlaceShips(&a) && WallshipAI_PlaceShips(&b));
        touching += verify_fleet(&a);
        assert(memcmp(a.own_board, b.own_board, sizeof a.own_board) == 0);
        assert(a.rng_state == b.rng_state);
        if (memcmp(first_board, a.own_board, sizeof first_board) != 0) { ++varied; }
    }
    assert(varied > 900 && touching > 0);
    a.profile = AI_AGGRESSIVE;
    a.human_shot_board[0][0] = 1;
    a.enemy_board[0][0] = CELL_HIT;
    a.remaining_ship_3 = 0;
    a.awaiting_sunk_details = true;
    WallshipAI_NewGame(&a, 0);
    assert(a.profile == AI_AGGRESSIVE && a.rng_state == WALLSHIP_DEFAULT_SEED);
    assert(a.enemy_board[0][0] == CELL_UNKNOWN && a.own_board[0][0] == 0);
    assert(a.human_shot_board[0][0] == 0 && a.remaining_ship_3 == 2);
    assert(!a.last_shot_valid && !a.awaiting_sunk_details);
    puts("Placement: 1000 seeds, fleet geometry, overlap, touching, reproducibility passed.");
}

static void test_human_shots(void)
{
    WallshipAgent ai;
    uint8_t length = 99;
    int i, x;
    WallshipAI_Init(&ai, 1, AI_RANDOM);
    /* Two touching horizontal ships: AAA BB, with independent IDs. */
    for (i = 0; i < 2; ++i) {
        Ship *s = &ai.ships[i];
        s->length = (uint8_t)(3-i);
        s->hp = s->length;
        s->start_x = (uint8_t)(i == 0 ? 0 : 3);
        for (x = 0; x < s->length; ++x) {
            ai.own_board[0][s->start_x+x] = s->id;
        }
    }
    assert(WallshipAI_ProcessHumanShot(&ai, 7, 6, &length) == SHOT_MISS);
    assert(length == 0);
    assert(WallshipAI_ProcessHumanShot(&ai, 7, 6, &length) == SHOT_INVALID);
    assert(WallshipAI_ProcessHumanShot(&ai, 0, 0, &length) == SHOT_HIT);
    assert(ai.ships[0].hp == 2 && length == 0);
    assert(WallshipAI_ProcessHumanShot(&ai, 0, 0, &length) == SHOT_INVALID);
    assert(ai.ships[0].hp == 2);
    assert(WallshipAI_ProcessHumanShot(&ai, 1, 0, NULL) == SHOT_HIT);
    assert(WallshipAI_ProcessHumanShot(&ai, 2, 0, &length) == SHOT_SUNK);
    assert(length == 3 && ai.ships[0].sunk && ai.ships[0].hp == 0);
    assert(ai.ships[1].hp == 2 && !ai.ships[1].sunk);
    assert(WallshipAI_ProcessHumanShot(&ai, 2, 0, &length) == SHOT_INVALID);
    assert(length == 0);
    assert(WallshipAI_ProcessHumanShot(&ai, 3, 0, NULL) == SHOT_HIT);
    assert(WallshipAI_ProcessHumanShot(&ai, 4, 0, &length) == SHOT_SUNK);
    assert(length == 2 && ai.ships[1].sunk);
    assert(WallshipAI_ProcessHumanShot(&ai, 8, 0, &length) == SHOT_INVALID);
    assert(WallshipAI_ProcessHumanShot(&ai, 0, 7, &length) == SHOT_INVALID);
    assert(WallshipAI_ProcessHumanShot(&ai, 255, 255, &length) == SHOT_INVALID);
    assert(length == 0);
    assert(!WallshipAI_ShouldContinueTurn(SHOT_MISS));
    assert(WallshipAI_ShouldContinueTurn(SHOT_HIT));
    assert(WallshipAI_ShouldContinueTurn(SHOT_SUNK));
    assert(!WallshipAI_ShouldContinueTurn(SHOT_INVALID));
}

static void test_shot_selection(void)
{
    WallshipAgent ai, twin;
    unsigned char snapshot[sizeof(WallshipAgent)];
    int profile, i;
    for (profile = AI_RANDOM; profile <= AI_AGGRESSIVE; ++profile) {
        bool seen[BOARD_H][BOARD_W] = {{false}};
        WallshipAI_Init(&ai, 123, (AIProfile)profile);
        WallshipAI_Init(&twin, 123, (AIProfile)profile);
        for (i = 0; i < BOARD_W * BOARD_H; ++i) {
            const Position p = WallshipAI_SelectShot(&ai);
            const Position q = WallshipAI_SelectShot(&twin);
            assert(inside(p) && p.x == q.x && p.y == q.y);
            assert(!seen[p.y][p.x] && ai.enemy_board[p.y][p.x] == CELL_UNKNOWN);
            seen[p.y][p.x] = true;
            memcpy(snapshot, &ai, sizeof ai);
            assert_invalid(WallshipAI_SelectShot(&ai));
            WallshipAI_ReportShotResult(&ai, p.x, p.y, SHOT_INVALID);
            WallshipAI_ReportShotResult(&ai, (uint8_t)((p.x+1) % BOARD_W), p.y, SHOT_MISS);
            WallshipAI_ReportShotResult(&ai, 255, 255, SHOT_HIT);
            assert(memcmp(&ai, snapshot, sizeof ai) == 0);
            WallshipAI_ReportShotResult(&ai, p.x, p.y, SHOT_MISS);
            WallshipAI_ReportShotResult(&twin, q.x, q.y, SHOT_MISS);
            assert(ai.enemy_board[p.y][p.x] == CELL_MISS && !ai.last_shot_valid);
            WallshipAI_ReportShotResult(&ai, p.x, p.y, SHOT_HIT);
            assert(ai.enemy_board[p.y][p.x] == CELL_MISS);
        }
        assert_invalid(WallshipAI_SelectShot(&ai));
        /* Only corners remain selectable; HIT and SUNK must stay excluded. */
        WallshipAI_NewGame(&ai, 50);
        fill_enemy(&ai, CELL_MISS);
        ai.enemy_board[3][3] = CELL_HIT;
        ai.enemy_board[4][4] = CELL_SUNK;
        ai.enemy_board[0][0] = CELL_UNKNOWN;
        {
            Position p = WallshipAI_SelectShot(&ai);
            assert(p.x == 0 && p.y == 0);
            WallshipAI_ReportShotResult(&ai, p.x, p.y, SHOT_MISS);
            ai.enemy_board[6][7] = CELL_UNKNOWN;
            p = WallshipAI_SelectShot(&ai);
            assert(p.x == 7 && p.y == 6);
            WallshipAI_ReportShotResult(&ai, p.x, p.y, SHOT_MISS);
            assert_invalid(WallshipAI_SelectShot(&ai));
        }
    }
}

static void test_targeting(void)
{
    WallshipAgent ai;
    int profile, seed, corner;
    for (profile = AI_BALANCED; profile <= AI_AGGRESSIVE; ++profile) {
        for (seed = 1; seed <= 100; ++seed) {
            Position p;
            WallshipAI_Init(&ai, (uint32_t)seed, (AIProfile)profile);
            ai.enemy_board[3][3] = CELL_HIT;
            p = WallshipAI_SelectShot(&ai);
            assert((p.y == 3 && (p.x == 2 || p.x == 4)) ||
                   (p.x == 3 && (p.y == 2 || p.y == 4)));
            WallshipAI_NewGame(&ai, (uint32_t)seed);
            ai.enemy_board[3][3] = ai.enemy_board[3][4] = CELL_HIT;
            p = WallshipAI_SelectShot(&ai);
            assert(p.y == 3 && (p.x == 2 || p.x == 5));
            WallshipAI_NewGame(&ai, (uint32_t)seed);
            ai.enemy_board[2][4] = ai.enemy_board[3][4] = CELL_HIT;
            p = WallshipAI_SelectShot(&ai);
            assert(p.x == 4 && (p.y == 1 || p.y == 4));
            WallshipAI_NewGame(&ai, (uint32_t)seed);
            ai.enemy_board[3][2] = ai.enemy_board[3][4] = CELL_HIT;
            p = WallshipAI_SelectShot(&ai);
            assert(p.x == 3 && p.y == 3);
            for (corner = 0; corner < 4; ++corner) {
                const int x = (corner & 1) != 0 ? 7 : 0;
                const int y = (corner & 2) != 0 ? 6 : 0;
                const int nx = x == 0 ? 1 : 6;
                const int ny = y == 0 ? 1 : 5;
                WallshipAI_NewGame(&ai, (uint32_t)seed);
                ai.enemy_board[y][x] = CELL_HIT;
                p = WallshipAI_SelectShot(&ai);
                assert((p.x == nx && p.y == y) || (p.x == x && p.y == ny));
                WallshipAI_NewGame(&ai, (uint32_t)seed);
                ai.enemy_board[y][x] = ai.enemy_board[y][nx] = CELL_HIT;
                p = WallshipAI_SelectShot(&ai);
                assert(p.y == y && p.x == (x == 0 ? 2 : 5));
                WallshipAI_NewGame(&ai, (uint32_t)seed);
                ai.enemy_board[y][x] = ai.enemy_board[ny][x] = CELL_HIT;
                p = WallshipAI_SelectShot(&ai);
                assert(p.x == x && p.y == (y == 0 ? 2 : 4));
            }
            /* Four adjacent hits must not be treated as one impossible ship. */
            WallshipAI_NewGame(&ai, (uint32_t)seed);
            ai.enemy_board[3][2] = ai.enemy_board[3][3] = CELL_HIT;
            ai.enemy_board[3][4] = ai.enemy_board[3][5] = CELL_HIT;
            p = WallshipAI_SelectShot(&ai);
            assert(p.y == 3 && (p.x == 1 || p.x == 6));
            /* An extension through a blocker cannot earn a target bonus. */
            WallshipAI_NewGame(&ai, (uint32_t)seed);
            ai.enemy_board[3][3] = ai.enemy_board[3][4] = CELL_HIT;
            ai.enemy_board[3][2] = CELL_MISS;
            ai.enemy_board[3][5] = CELL_SUNK;
            p = WallshipAI_SelectShot(&ai);
            assert((p.x == 3 || p.x == 4) && (p.y == 2 || p.y == 4));
        }
    }
}

static void rejected_sunk(WallshipAgent *ai, uint8_t length,
                          const Position *cells, uint8_t count)
{
    unsigned char before[sizeof(WallshipAgent)];
    memcpy(before, ai, sizeof before);
    WallshipAI_ReportSunkShip(ai, length, cells, count);
    assert(memcmp(before, ai, sizeof before) == 0);
}

static void test_sunk_events(void)
{
    WallshipAgent ai;
    const Position cells[3] = {{4, 3}, {2, 3}, {3, 3}};
    const Position duplicate[3] = {{4, 3}, {4, 3}, {3, 3}};
    const Position outside[3] = {{4, 3}, {255, 3}, {3, 3}};
    const Position bent[3] = {{4, 3}, {3, 3}, {3, 4}};
    const Position gap[3] = {{4, 3}, {2, 3}, {1, 3}};
    const Position no_last[3] = {{1, 3}, {2, 3}, {3, 3}};
    int length;
    WallshipAI_Init(&ai, 31, AI_BALANCED);
    ai.enemy_board[3][1] = ai.enemy_board[3][2] = ai.enemy_board[3][3] = CELL_HIT;
    ai.enemy_board[4][3] = CELL_HIT;
    rejected_sunk(&ai, 3, cells, 3); /* Details before SHOT_SUNK are rejected. */
    pending_sunk(&ai, 4, 3);
    assert_invalid(WallshipAI_SelectShot(&ai));
    rejected_sunk(&ai, 3, NULL, 3);
    rejected_sunk(&ai, 0, cells, 0);
    rejected_sunk(&ai, 4, cells, 3);
    rejected_sunk(&ai, 3, cells, 2);
    rejected_sunk(&ai, 3, duplicate, 3);
    rejected_sunk(&ai, 3, outside, 3);
    rejected_sunk(&ai, 3, bent, 3);
    rejected_sunk(&ai, 3, gap, 3);
    rejected_sunk(&ai, 3, no_last, 3);
    ai.enemy_board[3][2] = CELL_UNKNOWN;
    rejected_sunk(&ai, 3, cells, 3);
    ai.enemy_board[3][2] = CELL_MISS;
    rejected_sunk(&ai, 3, cells, 3);
    ai.enemy_board[3][2] = CELL_SUNK;
    rejected_sunk(&ai, 3, cells, 3);
    ai.enemy_board[3][2] = CELL_HIT;
    ai.remaining_ship_3 = 0;
    rejected_sunk(&ai, 3, cells, 3);
    ai.remaining_ship_3 = 2;
    WallshipAI_ReportShotResult(&ai, 4, 3, SHOT_MISS);
    assert(ai.enemy_board[3][4] == CELL_HIT && ai.awaiting_sunk_details);
    WallshipAI_ReportSunkShip(&ai, 3, cells, 3);
    assert(ai.remaining_ship_3 == 1 && !ai.last_shot_valid && !ai.awaiting_sunk_details);
    assert(ai.enemy_board[3][2] == CELL_SUNK && ai.enemy_board[3][3] == CELL_SUNK);
    assert(ai.enemy_board[3][4] == CELL_SUNK);
    assert(ai.enemy_board[3][1] == CELL_HIT && ai.enemy_board[4][3] == CELL_HIT);
    rejected_sunk(&ai, 3, cells, 3);
    {
        Position p = WallshipAI_SelectShot(&ai);
        assert((p.x == 0 && p.y == 3) || (p.x == 1 && (p.y == 2 || p.y == 4)) ||
               (p.y == 4 && (p.x == 2 || p.x == 4)) || (p.x == 3 && p.y == 5));
    }
    for (length = 1; length <= 3; ++length) {
        Position vertical[3] = {{7, 6}, {7, 5}, {7, 4}};
        int i;
        WallshipAI_NewGame(&ai, 1);
        for (i = 1; i < length; ++i) {
            ai.enemy_board[vertical[i].y][vertical[i].x] = CELL_HIT;
        }
        pending_sunk(&ai, 7, 6);
        WallshipAI_ReportSunkShip(&ai, (uint8_t)length, vertical, (uint8_t)length);
        assert(!ai.awaiting_sunk_details);
        assert(ai.remaining_ship_1 == (length == 1 ? 1 : 2));
        assert(ai.remaining_ship_2 == (length == 2 ? 1 : 2));
        assert(ai.remaining_ship_3 == (length == 3 ? 1 : 2));
    }
}

static void test_random_and_ties(void)
{
    WallshipAgent ai;
    unsigned left[3] = {0, 0, 0}, right[3] = {0, 0, 0};
    int profile;
    uint32_t seed;
    for (profile = AI_RANDOM; profile <= AI_AGGRESSIVE; ++profile) {
        for (seed = 1; seed <= 500; ++seed) {
            Position p;
            WallshipAI_Init(&ai, seed, (AIProfile)profile);
            fill_enemy(&ai, CELL_MISS);
            ai.enemy_board[0][0] = ai.enemy_board[6][7] = CELL_UNKNOWN;
            p = WallshipAI_SelectShot(&ai);
            if (p.x == 0 && p.y == 0) { ++left[profile]; }
            else { assert(p.x == 7 && p.y == 6); ++right[profile]; }
        }
        assert(left[profile] > 100 && right[profile] > 100);
    }
}

static uint8_t get_ship_cells(const WallshipAgent *defender, uint8_t id,
                              Position cells[MAX_SHIP_LENGTH])
{
    uint8_t count = 0;
    int x, y;
    for (y = 0; y < BOARD_H; ++y) {
        for (x = 0; x < BOARD_W; ++x) {
            if (defender->own_board[y][x] == id) {
                assert(count < MAX_SHIP_LENGTH);
                cells[count].x = (uint8_t)x;
                cells[count].y = (uint8_t)y;
                ++count;
            }
        }
    }
    return count;
}

static void test_simulated_games(void)
{
    int profile;
    uint32_t game;
    for (profile = AI_RANDOM; profile <= AI_AGGRESSIVE; ++profile) {
        unsigned total_shots = 0;
        for (game = 1; game <= 1000; ++game) {
            WallshipAgent attacker, defender;
            bool seen[BOARD_H][BOARD_W] = {{false}};
            unsigned shots = 0, sinks = 0;
            uint8_t left[4] = {0, 2, 2, 2};
            WallshipAI_Init(&attacker, game * UINT32_C(7919), (AIProfile)profile);
            WallshipAI_Init(&defender, game * UINT32_C(104729), AI_RANDOM);
            assert(WallshipAI_PlaceShips(&defender));
            while (sinks < SHIP_COUNT) {
                const Position p = WallshipAI_SelectShot(&attacker);
                uint8_t length = 0;
                ShotResult result;
                assert(inside(p) && !seen[p.y][p.x] && shots < 56);
                seen[p.y][p.x] = true;
                ++shots;
                result = WallshipAI_ProcessHumanShot(&defender, p.x, p.y, &length);
                assert(result != SHOT_INVALID);
                WallshipAI_ReportShotResult(&attacker, p.x, p.y, result);
                if (result == SHOT_SUNK) {
                    Position cells[MAX_SHIP_LENGTH];
                    uint8_t count = get_ship_cells(&defender, defender.own_board[p.y][p.x], cells);
                    assert(count == length && length >= 1 && length <= 3);
                    assert_invalid(WallshipAI_SelectShot(&attacker));
                    WallshipAI_ReportSunkShip(&attacker, length, cells, count);
                    assert(!attacker.awaiting_sunk_details && !attacker.last_shot_valid);
                    assert(left[length] > 0);
                    --left[length];
                    ++sinks;
                } else {
                    assert(attacker.enemy_board[p.y][p.x] ==
                           (result == SHOT_HIT ? CELL_HIT : CELL_MISS));
                }
                assert(attacker.remaining_ship_1 == left[1]);
                assert(attacker.remaining_ship_2 == left[2]);
                assert(attacker.remaining_ship_3 == left[3]);
            }
            {
                int x, y, i;
                unsigned sunk_cells = 0;
                for (i = 0; i < SHIP_COUNT; ++i) {
                    assert(defender.ships[i].sunk && defender.ships[i].hp == 0);
                }
                for (y = 0; y < BOARD_H; ++y) {
                    for (x = 0; x < BOARD_W; ++x) {
                        if (attacker.enemy_board[y][x] == CELL_SUNK) { ++sunk_cells; }
                        assert(attacker.enemy_board[y][x] != CELL_HIT);
                    }
                }
                assert(sunk_cells == 12);
            }
            total_shots += shots;
        }
        printf("Profile %d: 1000 victories; mean shots %u.%03u (maximum allowed 56).\n",
               profile, total_shots / 1000, total_shots % 1000);
    }
}

static void test_null_arguments(void)
{
    uint8_t length = 55;
    WallshipAI_Init(NULL, 0, AI_RANDOM);
    WallshipAI_NewGame(NULL, 0);
    assert(!WallshipAI_PlaceShips(NULL));
    assert_invalid(WallshipAI_SelectShot(NULL));
    WallshipAI_ReportShotResult(NULL, 0, 0, SHOT_HIT);
    WallshipAI_ReportSunkShip(NULL, 1, NULL, 1);
    assert(WallshipAI_ProcessHumanShot(NULL, 0, 0, &length) == SHOT_INVALID);
    assert(length == 0);
}

int main(void)
{
    test_initialization_and_placement();
    test_human_shots();
    test_shot_selection();
    test_targeting();
    test_sunk_events();
    test_random_and_ties();
    test_simulated_games();
    test_null_arguments();
    printf("RAM (this ABI): agent=%lu enemy=%lu own=%lu human=%lu ships=%lu score=%lu bytes\n",
           (unsigned long)sizeof(WallshipAgent),
           (unsigned long)sizeof(((WallshipAgent *)0)->enemy_board),
           (unsigned long)sizeof(((WallshipAgent *)0)->own_board),
           (unsigned long)sizeof(((WallshipAgent *)0)->human_shot_board),
           (unsigned long)sizeof(((WallshipAgent *)0)->ships),
           (unsigned long)(sizeof(int32_t) * BOARD_H * BOARD_W));
    puts("All Wallship AI tests passed.");
    return 0;
}
```

## 5. STM32 integration example

The template uses a single game and a single fixed-size mailbox. Call Poll on every main-loop iteration; it handles at most one event and issues at most one new attack per call. No blocking gameplay loop waits for a result. The ordinary firmware superloop continues servicing other peripherals.

MISS transfers the turn. HIT schedules another shot immediately. SUNK waits for validated details and then schedules another shot. Victory is checked before the extra turn, for either player. A placement failure or exhausted board with inconsistent enemy counters produces GAME_ERROR. A malformed human shot is reported as SHOT_INVALID and leaves the human turn active.

The transport must echo the transaction ID for both AI-result and SUNK-detail messages. IDs advance across new games in the same process; stop/drain old transport traffic before starting a game, after reset, or before ID reuse. Human input must be gated to the active human turn. Transport hooks must queue/copy data before returning, be nonblocking, and manage link retries/timeouts outside the core.

`examples/wallship_game.h`:

```c
#ifndef WALLSHIP_GAME_H
#define WALLSHIP_GAME_H

#include "wallship_ai.h"

typedef enum {
    GAME_AGENT_TURN,
    GAME_WAIT_AGENT_RESULT,
    GAME_HUMAN_TURN,
    GAME_OVER,
    GAME_ERROR
} GameState;

typedef enum { EVENT_AGENT_RESULT, EVENT_SUNK_DETAILS, EVENT_HUMAN_SHOT } GameEventKind;

typedef struct {
    GameEventKind kind;
    uint32_t transaction;
    uint8_t x;
    uint8_t y;
    ShotResult result;
    uint8_t ship_length;
    uint8_t cell_count;
    Position cells[MAX_SHIP_LENGTH];
} GameEvent;

typedef struct {
    WallshipAgent ai;
    GameState state;
    uint32_t transaction;
    bool details_ready;
    uint8_t sunk_length;
    uint8_t sunk_count;
    Position sunk_cells[MAX_SHIP_LENGTH];
} WallshipGame;

/* Single active game/mailbox. Start from main context with transport stopped.
 * first_turn must be GAME_AGENT_TURN or GAME_HUMAN_TURN.
 */
bool WallshipGame_Start(WallshipGame *game, uint32_t seed, AIProfile profile,
                        GameState first_turn);
void WallshipGame_Poll(WallshipGame *game);

/* ISR-safe fixed-size capture, no AI work. Returns false if full/invalid.
 * Producer must arrange retransmission when false; do not overwrite a slot.
 * Use the transaction supplied to App_SendShot for both AI reply events.
 */
bool WallshipGame_CaptureFromISR(const GameEvent *event);
bool WallshipGame_TakeOverflowFlag(void);

/* Application hooks: see STM32 adapter in the guide. IRQ save/restore must
 * also provide compiler memory barriers. All other hooks run in main context.
 */
uint32_t App_IrqSave(void);
void App_IrqRestore(uint32_t saved);
void App_SendShot(uint32_t transaction, uint8_t x, uint8_t y);
void App_SendHumanResult(uint8_t x, uint8_t y, ShotResult result,
                        uint8_t ship_length, const Position *cells, uint8_t count);
void App_GameFinished(bool agent_won);

#endif
```

`examples/wallship_game.c`:

```c
#include "wallship_game.h"

#include <stddef.h>
#include <string.h>

/* Accessed by ISR and main only inside saved/restored IRQ critical sections.
 * volatile forces observable accesses; the critical section provides exclusion.
 */
static volatile GameEvent mailbox;
static volatile uint8_t mailbox_pending;
static volatile uint8_t mailbox_overflow;
static uint32_t next_transaction;

bool WallshipGame_CaptureFromISR(const GameEvent *event)
{
    uint32_t saved;
    if (event == NULL || event->cell_count > MAX_SHIP_LENGTH ||
        event->kind < EVENT_AGENT_RESULT || event->kind > EVENT_HUMAN_SHOT) {
        return false;
    }
    saved = App_IrqSave();
    if (mailbox_pending != 0) {
        mailbox_overflow = 1;
        App_IrqRestore(saved);
        return false;
    }
    mailbox = *event;
    mailbox_pending = 1; /* Publish only after the complete payload is copied. */
    App_IrqRestore(saved);
    return true;
}

static bool take_event(GameEvent *event)
{
    const uint32_t saved = App_IrqSave();
    const bool ready = mailbox_pending != 0;
    if (ready) {
        *event = mailbox;
        mailbox_pending = 0;
    }
    App_IrqRestore(saved);
    return ready;
}

bool WallshipGame_TakeOverflowFlag(void)
{
    const uint32_t saved = App_IrqSave();
    const bool overflow = mailbox_overflow != 0;
    mailbox_overflow = 0;
    App_IrqRestore(saved);
    return overflow;
}

bool WallshipGame_Start(WallshipGame *game, uint32_t seed, AIProfile profile,
                        GameState first_turn)
{
    uint32_t saved;
    if (game == NULL) { return false; }
    memset(game, 0, sizeof *game);
    game->state = GAME_ERROR;
    if (first_turn != GAME_AGENT_TURN && first_turn != GAME_HUMAN_TURN) {
        return false;
    }
    saved = App_IrqSave();
    mailbox_pending = 0;
    mailbox_overflow = 0;
    App_IrqRestore(saved);
    WallshipAI_Init(&game->ai, seed, profile);
    if (!WallshipAI_PlaceShips(&game->ai)) {
        return false;
    }
    game->state = first_turn;
    return true;
}

static void advance_agent_turn(WallshipGame *game, ShotResult result)
{
    if (game->ai.remaining_ship_1 == 0 && game->ai.remaining_ship_2 == 0 &&
        game->ai.remaining_ship_3 == 0) {
        game->state = GAME_OVER;
        App_GameFinished(true);
    } else {
        game->state = WallshipAI_ShouldContinueTurn(result)
            ? GAME_AGENT_TURN : GAME_HUMAN_TURN;
    }
}

static void try_sunk_details(WallshipGame *game)
{
    if (game->ai.awaiting_sunk_details && game->details_ready) {
        WallshipAI_ReportSunkShip(&game->ai, game->sunk_length,
                                  game->sunk_cells, game->sunk_count);
        game->details_ready = false;
        if (!game->ai.awaiting_sunk_details) {
            advance_agent_turn(game, SHOT_SUNK);
        }
        /* Rejected details leave WAIT active; accept a corrected event later. */
    }
}

static void handle_agent_event(WallshipGame *game, const GameEvent *event)
{
    if (event->transaction != game->transaction) { return; }
    if (event->kind == EVENT_SUNK_DETAILS) {
        if (event->ship_length < 1 || event->ship_length > MAX_SHIP_LENGTH ||
            event->cell_count != event->ship_length) { return; }
        game->sunk_length = event->ship_length;
        game->sunk_count = event->cell_count;
        memcpy(game->sunk_cells, event->cells, sizeof game->sunk_cells);
        game->details_ready = true;
        try_sunk_details(game);
    } else if (event->kind == EVENT_AGENT_RESULT) {
        WallshipAI_ReportShotResult(&game->ai, event->x, event->y, event->result);
        if (game->ai.awaiting_sunk_details) {
            try_sunk_details(game);
        } else if (!game->ai.last_shot_valid) {
            game->details_ready = false;
            advance_agent_turn(game, event->result);
        }
    }
}

static void handle_human_shot(WallshipGame *game, const GameEvent *event)
{
    Position cells[MAX_SHIP_LENGTH] = {{0, 0}};
    uint8_t length = 0, count = 0;
    const ShotResult result = WallshipAI_ProcessHumanShot(&game->ai, event->x,
                                                         event->y, &length);
    int i;
    bool all_sunk = true;
    if (result == SHOT_SUNK) {
        const Ship *s = &game->ai.ships[game->ai.own_board[event->y][event->x]-1];
        for (count = 0; count < length; ++count) {
            cells[count].x = (uint8_t)(s->start_x +
                (s->orientation == SHIP_HORIZONTAL ? count : 0));
            cells[count].y = (uint8_t)(s->start_y +
                (s->orientation == SHIP_VERTICAL ? count : 0));
        }
    }
    /* App hook must copy cells before returning if its transport is async. */
    App_SendHumanResult(event->x, event->y, result, length, cells, count);
    if (result == SHOT_INVALID) { return; }
    for (i = 0; i < SHIP_COUNT; ++i) {
        if (!game->ai.ships[i].sunk) { all_sunk = false; }
    }
    if (all_sunk) {
        game->state = GAME_OVER;
        App_GameFinished(false);
    } else {
        game->state = WallshipAI_ShouldContinueTurn(result)
            ? GAME_HUMAN_TURN : GAME_AGENT_TURN;
    }
}

void WallshipGame_Poll(WallshipGame *game)
{
    GameEvent event;
    if (game == NULL || game->state == GAME_ERROR || game->state == GAME_OVER) {
        return;
    }
    if (take_event(&event)) {
        if (game->state == GAME_WAIT_AGENT_RESULT) {
            handle_agent_event(game, &event);
        } else if (game->state == GAME_HUMAN_TURN && event.kind == EVENT_HUMAN_SHOT) {
            handle_human_shot(game, &event);
        }
    }
    if (game->state == GAME_AGENT_TURN) {
        const Position target = WallshipAI_SelectShot(&game->ai);
        if (target.x == INVALID_COORD || target.y == INVALID_COORD) {
            /* Exhausted board without victory means inconsistent feedback. */
            game->state = GAME_ERROR;
            return;
        }
        ++next_transaction;
        if (next_transaction == 0) { ++next_transaction; }
        game->transaction = next_transaction;
        game->details_ready = false;
        game->state = GAME_WAIT_AGENT_RESULT;
        App_SendShot(game->transaction, target.x, target.y);
    }
}
```

Implement the App hooks in your board-specific translation unit. This adapter assumes a Cube-generated `main.h`, initialized `htim2`, and the application's transport/UI functions; those names are integration points, not portable core dependencies.

```c
#include "main.h"       /* Board HAL/CMSIS includes */
#include "wallship_game.h"

extern TIM_HandleTypeDef htim2;
static WallshipGame game; /* Persistent agent state belongs outside the stack. */

/* Application-owned, nonblocking hooks. Queue routines copy their arguments. */
extern void GameTransport_QueueShot(uint32_t id, uint8_t x, uint8_t y);
extern void GameTransport_QueueHumanResult(uint8_t x, uint8_t y, ShotResult result,
                                         uint8_t length, const Position *cells,
                                         uint8_t count);
extern void GameTransport_Poll(void);
extern void GameTransport_RequestReplay(void);
extern void DisplayWinner(bool agent_won);
extern void DisplayStartupError(void);
extern void DisplayGameError(void);

uint32_t App_IrqSave(void)
{
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    __DMB();
    return saved;
}

void App_IrqRestore(uint32_t saved)
{
    __DMB();
    __set_PRIMASK(saved);
}

void App_SendShot(uint32_t id, uint8_t x, uint8_t y)
{
    GameTransport_QueueShot(id, x, y); /* Send target.x and target.y. */
}

void App_SendHumanResult(uint8_t x, uint8_t y, ShotResult result,
                        uint8_t length, const Position *cells, uint8_t count)
{
    GameTransport_QueueHumanResult(x, y, result, length, cells, count);
}

void App_GameFinished(bool agent_won)
{
    DisplayWinner(agent_won);
}

bool GameApp_Start(void) /* Call after timer/HAL initialization. */
{
    uint32_t seed = HAL_GetTick() ^ (uint32_t)__HAL_TIM_GET_COUNTER(&htim2);
    if (!WallshipGame_Start(&game, seed, AI_BALANCED, GAME_AGENT_TURN)) {
        DisplayStartupError();
        return false; /* Do not start the transport/gameplay on this failure. */
    }
    return true;
}

void GameApp_Tick(void) /* Call once per firmware superloop iteration. */
{
    GameTransport_Poll();
    WallshipGame_Poll(&game);
    if (WallshipGame_TakeOverflowFlag()) {
        GameTransport_RequestReplay();
    }
    if (game.state == GAME_ERROR) {
        DisplayGameError();
    }
}
```

Use the target compiler's CMSIS intrinsics with compiler memory barriers; the host test IRQ hooks are only single-threaded simulations. Core initialization also protects against seed zero, but variable seed quality is the application's responsibility. Tick/time alone can repeat across identical startup timing.

## 6. SUNK interrupt/event example

An ISR only captures a bounded event. It does not call SelectShot, ReportShotResult, ReportSunkShip, or ProcessHumanShot. Here are transport-decoder callbacks invoked from an ISR after a complete validated packet has been received. The actual MCU handler must also acknowledge/clear its peripheral interrupt flag using the board's driver.

```c
#include "wallship_game.h"

bool CaptureShotResultFromISR(uint32_t transaction, uint8_t x, uint8_t y,
                              ShotResult result)
{
    GameEvent event = {0};
    event.kind = EVENT_AGENT_RESULT;
    event.transaction = transaction;
    event.x = x;
    event.y = y;
    event.result = result; /* SHOT_MISS, SHOT_HIT or SHOT_SUNK */
    return WallshipGame_CaptureFromISR(&event);
}

bool CaptureSunkFromISR(uint32_t transaction, uint8_t length,
                        const Position *cells, uint8_t count)
{
    GameEvent event = {0};
    uint8_t i;
    if (cells == 0 || length < 1 || length > MAX_SHIP_LENGTH || count != length) {
        return false;
    }
    event.kind = EVENT_SUNK_DETAILS;
    event.transaction = transaction;
    event.ship_length = length;
    event.cell_count = count;
    for (i = 0; i < count; ++i) {
        if (cells[i].x >= BOARD_W || cells[i].y >= BOARD_H) { return false; }
        event.cells[i] = cells[i];
    }
    return WallshipGame_CaptureFromISR(&event);
}

bool CaptureHumanShotFromISR(uint8_t x, uint8_t y)
{
    GameEvent event = {0};
    event.kind = EVENT_HUMAN_SHOT;
    event.x = x;
    event.y = y;
    return WallshipGame_CaptureFromISR(&event);
}
```

For the example ship `{(2,4),(3,4),(4,4)}`, report the final attack as SHOT_SUNK and send length 3, count 3, and those three coordinates under the same transaction ID. Once main accepts the details, it calls ReportSunkShip and immediately advances the turn unless that was the last enemy ship.

The one-slot mailbox must not receive two events without a main-loop drain between them. If full, capture returns false, preserves the queued event, and latches overflow. The transport must retain/replay an unaccepted event or use backpressure; do not acknowledge a rejected event as successfully queued. This applies when result and SUNK details arrive in one burst. Do not busy-wait for the mailbox inside the ISR. Malformed feedback also needs application-level correction/retry; without valid feedback the agent intentionally stays pending.

Both producer and consumer briefly save PRIMASK, disable maskable interrupts, copy fixed-size data, and restore the saved value. Main clears pending while still inside the critical section and processes its local copy after restoring interrupts. Therefore a new event can arrive during heuristic calculation without overwriting the event being processed. Masking the producer also prevents a nested maskable ISR producer from interleaving writes. NMI/HardFault must not produce these events. This design targets a single Cortex-M core; a multicore shared mailbox requires different synchronization.

`volatile` ensures accesses occur but does not make a multi-field payload atomic or prevent lost updates. Publishing pending last, critical sections on both ends, and compiler/hardware barriers together provide the intended ordering. Restoring the previous PRIMASK avoids accidentally enabling interrupts in an already-masked caller. Validate wider wire-format coordinates and lengths before narrowing to uint8_t, then let the core validate semantic SUNK geometry.

## 7. Heuristic scoring explanation

For each still-remaining length, enumerate all horizontal and vertical placements; enumerate a length-one placement only once. A placement must be in bounds and contain only UNKNOWN or HIT. Each UNKNOWN cell in a valid placement receives `hunt_weight * remaining_count`. Existing HIT cells are evidence, never attack candidates. SUNK cells block overlap, but no halo is blocked because ships may touch.

| Profile | Hunt weight | Adjacent bonus | Line bonus | Jitter, inclusive |
| --- | ---: | ---: | ---: | ---: |
| Balanced | 10 | 400 | 1200 | 0..5 |
| Aggressive | 6 | 800 | 2000 | 0..3 |

The two configurations returned by `WallshipAI_ScoreConfig` in the header are the tuning points. AI_RANDOM uses uniform reservoir sampling of UNKNOWN cells, bypassing scoring. Heuristic selection also uses reservoir sampling for exact maximum-score ties. It does not favor the first board coordinate. Both jitter and tie breaking use the agent's own RNG state.

An interior cell participates in at most 1, 4, and 6 placements of lengths 1, 2, and 3 respectively. With two of each length remaining, maximum hunt contribution is `22 * hunt_weight`: 220 balanced or 132 aggressive. One adjacent bonus exceeds that plus jitter, and one line bonus exceeds adjacent plus hunt plus jitter. The highest target level is applied only once per cell. With valid game state, maximum scores are 1425 balanced and 2135 aggressive, safely within int32_t.

These scores measure independent placement density, not an exact posterior over complete nonoverlapping fleets. Full-fleet consistency enumeration is deliberately unnecessary for this small heuristic agent.

## 8. HIT target-mode explanation

A single unresolved HIT grants an adjacent bonus only to an orthogonal UNKNOWN neighbor that shares a legal remaining-ship placement with it. Diagonal cells receive no target bonus. If no length 2 or 3 ship remains, there is no speculative multi-cell target placement.

Two aligned hits inside a candidate length-three placement grant its remaining UNKNOWN cell the line bonus. This favors both ends of `? H H ?` and the middle of `H ? H`, including vertical equivalents. Sideways candidates remain available at lower priority because touching ships can create misleading alignments. Boundaries, MISS and SUNK can remove either extension. The code never concludes from alignment alone that the hits must be the same ship.

Target level uses a temporary byte map: 0 hunt, 1 adjacent, 2 aligned pair. A maximum operation prevents stacked bonuses around touching ships from overwhelming the intended priority levels. Jitter cannot move an ordinary hunt cell ahead of a feasible target cell.

## 9. Touching-ship handling explanation

Own cells contain ship IDs 1..6, not booleans. A human hit at a fresh occupied cell decrements only that ID's HP. Human-shot history prevents repeat damage; repeats return SHOT_INVALID. A ship sinks exactly when HP reaches zero.

Enemy HIT cells are never flood-filled into permanent ship groups. A row of four hits can contain two length-two ships, a length-three ship touching a singleton, or pieces of other ships. Each hypothetical placement is considered independently of adjacent hits outside its footprint.

Only a valid SUNK event resolves identity. Validate all coordinates before any mutation: 1..3 matching unique cells, all HIT and in bounds, straight and contiguous in either orientation, including the pending shot, with a remaining ship of that length. Order of coordinates does not matter. Mark exactly those cells SUNK and decrement once. Nearby HIT cells continue targeting, and neighboring UNKNOWN cells remain shootable. Invalid, duplicate, partial, or zero-counter reports change nothing.

## 10. Random ship placement explanation

Place ships in order 3,3,2,2,1,1 with IDs 1..6. Each attempt draws independent x, y and orientation values. Out-of-bounds and overlapping proposals are rejected; touching is allowed. Singleton orientation is normalized to horizontal after acceptance because it has no geometric effect.

Each ship gets at most 100 attempts. The complete fleet gets at most 10 attempts total (the initial attempt plus at most nine restarts). Between attempts, clear the own board, human-shot history and ship HP records. On final failure clear partial state and return false. PlaceShips is a setup-only operation and must not be called during active gameplay.

The RNG is xorshift32 with shifts 13,17,5. Seed zero becomes `0x6D2B79F5`. Bounded draws use rejection sampling against the nonzero xorshift output range rather than biased direct modulo reduction. The placement routine never reads enemy knowledge, human shooting history, a heatmap, or heuristic scores. Repeating the seed and sequence of calls reproduces placement and shots. Different seeds usually produce different layouts; there is no claim of uniform sampling across all complete fleets or cryptographic randomness.

## 11. Memory usage

Measured with the available 32-bit MinGW GCC ABI (four-byte enums). A conventional STM32 ABI with the same enum sizes/alignment is expected to have the same structure sizes; verify with your actual compiler and flags, especially `-fshort-enums`.

| Storage | Measured size | Lifetime |
| --- | ---: | --- |
| enemy_board, CellState[7][8] | 224 bytes | Inside agent |
| own_board, uint8_t[7][8] | 56 bytes | Inside agent |
| human_shot_board, uint8_t[7][8] | 56 bytes | Inside agent |
| ships[6], 12-byte Ship records | 72 bytes | Inside agent |
| Counters, profile, RNG, last shot, flags, padding | 16 bytes | Inside agent |
| **WallshipAgent total** | **424 bytes** | Persistent |
| Score map, int32_t[7][8] | 224 bytes | SelectShot scratch |
| Target-level map, uint8_t[7][8] | 56 bytes | Scoring scratch |
| WallshipGame including agent | 444 bytes | Persistent example state |
| GameEvent | 24 bytes | Mailbox or local copy |

The example adds a 24-byte static volatile mailbox, two one-byte flags, and a four-byte transaction counter: 30 bytes before linker alignment. Header-only functions can be emitted in each translation unit that uses them, so inspect the target link map for flash use. There is no dynamic memory and no large candidate-position array. The byte human-shot board is deduplication history, not a heatmap.

On the available 32-bit MinGW GCC build, `-O2 -fstack-usage` reports a **416-byte SelectShot frame** in the integration object and a **96-byte PlaceShips frame** in the AI test object. `WallshipRules_ProcessShot` has a 20-byte frame there. The compiler may inline other header functions into their callers; these measurements are individual frames, not whole-program worst-case bounds. Small loop variables, RNG state copies and tie counters account for compiler-dependent overhead beyond the 280-byte score/target scratch arrays.

The example's Poll frame is 160 bytes; its synchronous path into SelectShot has approximately `160 + 416 = 576` bytes of application call frames on this desktop build. Library routines, transport hooks, callers, interrupt exception frames, nested ISRs, optimization changes and ABI differences add further stack needs. Keep the persistent game/agent static. For STM32, generate `.su` files with the actual cross-compiler, inspect the complete call graph, and measure stack high-water usage under interrupts before assigning a production stack size. No hardware stack bound is claimed here.

## 12. Computational complexity

Let `N = W*H`, maximum length `L = 3`, and ship count `S = 6`. Placement scoring enumerates up to two orientations for each length and board origin, examining at most `length` cells for validation and scoring: `O(N * sum(length))`, or `O(N*L^2)` when lengths 1..L are variable. For this fixed fleet it is linear in the 56-cell board.

There are at most `56 * 5 = 280` candidate-origin/direction combinations (56 singleton plus 112 each for lengths 2 and 3), including rejected boundary candidates. Hunt and target evidence share this enumeration. Jitter and reservoir selection each require one additional board pass. Random-profile selection is O(N). Scratch space is O(N), 280 explicit bytes for heuristic maps.

ReportShotResult and a human shot are O(1). SUNK validation is O(L^2) for uniqueness checks, followed by O(L) updates; with at most three cells this is constant bounded work. Placement performs at most `10 * 6 * 100 = 6000` candidate attempts, each validating at most three cells, plus clearing 56-cell boards at restarts. Bounded RNG rejection has negligible expected overhead for these small ranges. The state-machine Poll processes only one queued event and at most one selection per invocation; transport waiting never spins inside the core.

## 13. Future improvements

- Tune profile weights against a larger, fixed held-out set of fleet layouts; retain deterministic regression seeds.
- Add a small fixed-capacity event queue if the real transport cannot provide single-mailbox backpressure.
- Add explicit protocol status/error reporting if the application needs to distinguish malformed feedback from missing feedback without examining pending flags.
- Measure cycle counts and stack high-water use on the selected STM32 and verify transport retry/session handling.
- If RAM becomes constrained, explicitly store cell-state bytes or pack human-shot history into bits; preserve the current readable representation until measurements justify changing it.

No learning model, heatmap-based placement, or dynamic allocation is needed for these improvements.
