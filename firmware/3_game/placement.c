#include "placement.h"

#include "wallship_ai.h"

#include <string.h>

void Placement_Clear(Placement *pl, WallshipAgent *board)
{
    memset(pl, 0, sizeof *pl);
    pl->remaining[1] = 2;
    pl->remaining[2] = 2;
    pl->remaining[3] = 2;
    /* Resets own_board and presets ships[] ids/lengths {3,3,2,2,1,1}. */
    WallshipAI_ClearOwnBoard(board);
}

bool Placement_SelectLength(Placement *pl, uint8_t length)
{
    if (length < 1 || length > MAX_SHIP_LENGTH || pl->remaining[length] == 0) {
        return false;
    }
    if (length != pl->selected_length) {
        pl->pending_count = 0; /* switching type drops a half-placed ship */
        pl->dx = 0;
        pl->dy = 0;
    }
    pl->selected_length = length;
    return true;
}

bool Placement_IsPending(const Placement *pl, uint8_t x, uint8_t y)
{
    uint8_t i;
    for (i = 0; i < pl->pending_count; ++i) {
        if (pl->pending_start.x + pl->dx * i == x &&
            pl->pending_start.y + pl->dy * i == y) {
            return true;
        }
    }
    return false;
}

bool Placement_IsComplete(const Placement *pl)
{
    return pl->placed == SHIP_COUNT;
}

static void commit_ship(Placement *pl, WallshipAgent *board)
{
    const uint8_t length = pl->selected_length;
    Ship *ship = NULL;
    uint8_t i;
    for (i = 0; i < SHIP_COUNT; ++i) {
        if (board->ships[i].length == length && board->ships[i].hp == 0) {
            ship = &board->ships[i];
            break;
        }
    }
    if (ship == NULL) {
        return; /* cannot happen while remaining[] is consistent */
    }
    ship->start_x = pl->pending_start.x;
    ship->start_y = pl->pending_start.y;
    ship->orientation = pl->dy != 0 ? SHIP_VERTICAL : SHIP_HORIZONTAL;
    ship->hp = length;
    ship->sunk = false;
    for (i = 0; i < length; ++i) {
        board->own_board[ship->start_y + pl->dy * i][ship->start_x + pl->dx * i] = ship->id;
    }
    --pl->remaining[length];
    ++pl->placed;
    pl->pending_count = 0;
    pl->dx = 0;
    pl->dy = 0;
    /* Keep the same type selected while more of it remain. */
    if (pl->remaining[length] == 0) {
        pl->selected_length = 0;
    }
}

PlaceResult Placement_PressCell(Placement *pl, WallshipAgent *board,
                                uint8_t x, uint8_t y)
{
    if (pl->selected_length == 0) {
        return PLACE_NO_LENGTH;
    }
    if (x >= BOARD_W || y >= BOARD_H || board->own_board[y][x] != 0 ||
        Placement_IsPending(pl, x, y)) {
        return PLACE_REJECT;
    }
    if (pl->pending_count == 0) {
        pl->pending_start.x = x;
        pl->pending_start.y = y;
    } else if (pl->pending_count == 1) {
        const int ddx = (int)x - pl->pending_start.x;
        const int ddy = (int)y - pl->pending_start.y;
        if (!((ddx == 0 && (ddy == 1 || ddy == -1)) ||
              (ddy == 0 && (ddx == 1 || ddx == -1)))) {
            return PLACE_REJECT; /* not adjacent */
        }
        pl->dx = (int8_t)(ddx != 0 ? 1 : 0);
        pl->dy = (int8_t)(ddy != 0 ? 1 : 0);
        if (ddx < 0 || ddy < 0) {
            pl->pending_start.x = x;
            pl->pending_start.y = y;
        }
    } else {
        /* Must extend the locked line at either end. */
        const int sx = pl->pending_start.x, sy = pl->pending_start.y;
        if (x == sx - pl->dx && y == sy - pl->dy) {
            pl->pending_start.x = x;
            pl->pending_start.y = y;
        } else if (!(x == sx + pl->dx * pl->pending_count &&
                     y == sy + pl->dy * pl->pending_count)) {
            return PLACE_REJECT;
        }
    }
    ++pl->pending_count;
    if (pl->pending_count == pl->selected_length) {
        commit_ship(pl, board);
        return PLACE_SHIP_DONE;
    }
    return PLACE_PENDING;
}
