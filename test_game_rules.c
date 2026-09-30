#include "wallship_game_rules.h"

#include <assert.h>
#include <string.h>

static void setup_player(WallshipAgent *player)
{
    const uint8_t lengths[SHIP_COUNT] = {3, 3, 2, 2, 1, 1};
    int i, x;
    memset(player, 0, sizeof *player);
    for (i = 0; i < SHIP_COUNT; ++i) {
        Ship *ship = &player->ships[i];
        ship->id = (uint8_t)(i + 1);
        ship->length = lengths[i];
        ship->hp = lengths[i];
        ship->start_y = (uint8_t)i;
        ship->orientation = SHIP_HORIZONTAL;
        for (x = 0; x < lengths[i]; ++x) {
            player->own_board[i][x] = ship->id;
        }
    }
}

int main(void)
{
    WallshipAgent players[2];
    WallshipTwoPlayerGame game;
    uint8_t length = 99;
    int y, x;

    setup_player(&players[0]);
    setup_player(&players[1]);
    assert(!WallshipTwoPlayerGame_Init(NULL, &players[0], &players[1], 0));
    assert(!WallshipTwoPlayerGame_Init(&game, &players[0], &players[0], 0));
    assert(!WallshipTwoPlayerGame_Init(&game, &players[0], &players[1], 2));
    players[1].ships[0].hp = 0;
    assert(!WallshipTwoPlayerGame_Init(&game, &players[0], &players[1], 0));
    players[1].ships[0].hp = 3;
    players[1].own_board[0][2] = 0;
    players[1].own_board[6][2] = 1;
    assert(!WallshipTwoPlayerGame_Init(&game, &players[0], &players[1], 0));
    players[1].own_board[0][2] = 1;
    players[1].own_board[6][2] = 0;
    assert(WallshipTwoPlayerGame_Init(&game, &players[0], &players[1], 0));
    assert(game.winner == INVALID_COORD && !game.finished);

    assert(WallshipRules_PreviewShot(&players[1], 0, 0) == SHOT_HIT);
    assert(players[1].ships[0].hp == 3);
    assert(WallshipRules_PreviewShot(&players[1], 7, 6) == SHOT_MISS);
    assert(WallshipRules_PreviewShot(&players[1], 8, 0) == SHOT_INVALID);
    assert(WallshipTwoPlayerGame_Shoot(&game, 0, 7, 6, &length) == SHOT_MISS);
    assert(length == 0 && game.current_player == 1 && !game.finished);
    assert(WallshipTwoPlayerGame_Shoot(&game, 0, 0, 0, &length) == SHOT_INVALID);
    assert(players[1].ships[0].hp == 3);
    assert(WallshipTwoPlayerGame_Shoot(&game, 1, 8, 0, &length) == SHOT_INVALID);
    assert(game.current_player == 1);

    assert(WallshipTwoPlayerGame_Shoot(&game, 1, 0, 0, &length) == SHOT_HIT);
    assert(length == 0 && game.current_player == 1);
    assert(players[0].ships[0].hp == 2);
    assert(WallshipRules_PreviewShot(&players[0], 0, 0) == SHOT_INVALID);
    assert(WallshipTwoPlayerGame_Shoot(&game, 1, 0, 0, &length) == SHOT_INVALID);
    assert(players[0].ships[0].hp == 2 && game.current_player == 1);
    assert(WallshipTwoPlayerGame_Shoot(&game, 1, 1, 0, &length) == SHOT_HIT);
    assert(WallshipRules_PreviewShot(&players[0], 2, 0) == SHOT_SUNK);
    assert(players[0].ships[0].hp == 1);
    assert(WallshipTwoPlayerGame_Shoot(&game, 1, 2, 0, &length) == SHOT_SUNK);
    assert(length == 3 && players[0].ships[0].sunk && !game.finished);
    assert(!WallshipRules_AllShipsSunk(&players[0]));

    for (y = 1; y < SHIP_COUNT; ++y) {
        for (x = 0; x < players[0].ships[y].length; ++x) {
            const ShotResult result = WallshipTwoPlayerGame_Shoot(
                &game, 1, (uint8_t)x, (uint8_t)y, &length);
            if (x + 1 == players[0].ships[y].length) {
                assert(result == SHOT_SUNK);
                assert(length == players[0].ships[y].length);
            } else {
                assert(result == SHOT_HIT && length == 0);
            }
        }
    }
    assert(WallshipRules_AllShipsSunk(&players[0]));
    assert(game.finished && game.winner == 1 && game.current_player == 1);
    assert(WallshipTwoPlayerGame_Shoot(&game, 1, 1, 6, &length) == SHOT_INVALID);
    assert(length == 0 && players[0].human_shot_board[6][1] == 0);
    assert(!WallshipRules_AllShipsSunk(NULL));

    setup_player(&players[0]);
    setup_player(&players[1]);
    assert(WallshipTwoPlayerGame_Init(&game, &players[0], &players[1], 0));
    assert(WallshipTwoPlayerGame_Shoot(&game, 0, 0, 0, NULL) == SHOT_HIT);
    assert(game.current_player == 0);
    return 0;
}
