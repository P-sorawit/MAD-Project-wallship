#include "wallship_heuristic.h"

#include <assert.h>
#include <string.h>

/* This test uses the scoring header without the AI or game-rule headers. */
int main(void)
{
    WallshipAgent ai;
    int32_t scores[BOARD_H][BOARD_W];
    int x, y;

    memset(&ai, 0, sizeof ai);
    ai.profile = AI_BALANCED;
    ai.rng_state = WALLSHIP_DEFAULT_SEED;
    ai.remaining_ship_1 = 1;
    WallshipAI_CalculateScores(&ai, scores);
    for (y = 0; y < BOARD_H; ++y) {
        for (x = 0; x < BOARD_W; ++x) {
            assert(scores[y][x] >= 10 && scores[y][x] <= 15);
        }
    }

    memset(ai.enemy_board, 0, sizeof ai.enemy_board);
    ai.remaining_ship_1 = 0;
    ai.remaining_ship_3 = 1;
    ai.enemy_board[3][3] = CELL_HIT;
    ai.enemy_board[3][4] = CELL_HIT;
    WallshipAI_CalculateScores(&ai, scores);
    assert(scores[3][5] >= 1260 && scores[3][5] <= 1265);
    assert(scores[3][3] == 0);
    assert(scores[3][4] == 0);

    ai.profile = AI_AGGRESSIVE;
    WallshipAI_CalculateScores(&ai, scores);
    assert(scores[3][5] >= 2036 && scores[3][5] <= 2039);
    return 0;
}
