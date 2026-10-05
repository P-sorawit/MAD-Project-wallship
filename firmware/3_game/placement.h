/* Module 3 (part): ship placement rules for S2, HAL-free.
 * Player picks a length (BTN_SHIP_1..3), then presses cells one by one on the
 * fleet panel. Cells must be empty, adjacent, and in the direction locked by
 * the first two cells (no L-shapes). CANCEL clears the whole board.
 */
#ifndef PLACEMENT_H
#define PLACEMENT_H

#include <stdbool.h>
#include <stdint.h>

#include "wallship_types.h"

typedef enum {
    PLACE_REJECT = 0,   /* invalid cell: beep, nothing recorded */
    PLACE_NO_LENGTH,    /* no ship type selected yet */
    PLACE_PENDING,      /* cell accepted, ship not complete yet */
    PLACE_SHIP_DONE     /* last cell of a ship: ship written to own_board */
} PlaceResult;

typedef struct {
    uint8_t selected_length;              /* 0 = none */
    uint8_t remaining[MAX_SHIP_LENGTH + 1]; /* index = length */
    uint8_t placed;
    uint8_t pending_count;
    Position pending_start;               /* lowest x/y of pending cells */
    int8_t dx, dy;                        /* locked direction, 0/0 = none */
} Placement;

/* Resets the pool to 2x len3, 2x len2, 2x len1 and empties own_board. */
void Placement_Clear(Placement *pl, WallshipAgent *board);
bool Placement_SelectLength(Placement *pl, uint8_t length);
PlaceResult Placement_PressCell(Placement *pl, WallshipAgent *board,
                                uint8_t x, uint8_t y);
bool Placement_IsPending(const Placement *pl, uint8_t x, uint8_t y);
bool Placement_IsComplete(const Placement *pl);

#endif
