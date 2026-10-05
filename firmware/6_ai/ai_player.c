#include "ai_player.h"

bool AiPlayer_Setup(WallshipAgent *ai, uint32_t seed, AIProfile profile)
{
    WallshipAI_Init(ai, seed, profile);
    return WallshipAI_PlaceShips(ai);
}

ShotResult AiPlayer_TakeShot(WallshipAgent *ai, WallshipTwoPlayerGame *match,
                             uint8_t ai_index, Position *shot,
                             uint8_t *sunk_length)
{
    const WallshipAgent *defender = match->players[1U - ai_index];
    const Position p = WallshipAI_SelectShot(ai);
    ShotResult result;
    *sunk_length = 0;
    *shot = p;
    if (p.x == INVALID_COORD || p.y == INVALID_COORD) {
        return SHOT_INVALID;
    }
    result = WallshipTwoPlayerGame_Shoot(match, ai_index, p.x, p.y, sunk_length);
    if (result == SHOT_INVALID) {
        return result;
    }
    WallshipAI_ReportShotResult(ai, p.x, p.y, result);
    if (result == SHOT_SUNK) {
        /* The AI must learn every cell of the sunk ship before it may fire
         * again; otherwise SelectShot keeps returning {255,255}. */
        const Ship *ship = &defender->ships[defender->own_board[p.y][p.x] - 1];
        Position cells[MAX_SHIP_LENGTH];
        uint8_t i;
        for (i = 0; i < ship->length; ++i) {
            cells[i].x = (uint8_t)(ship->start_x + (ship->orientation == SHIP_HORIZONTAL ? i : 0));
            cells[i].y = (uint8_t)(ship->start_y + (ship->orientation == SHIP_VERTICAL ? i : 0));
        }
        WallshipAI_ReportSunkShip(ai, ship->length, cells, ship->length);
        if (ai->awaiting_sunk_details) {
            return SHOT_INVALID;
        }
    }
    return result;
}
