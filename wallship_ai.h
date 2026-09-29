#ifndef WALLSHIP_AI_H
#define WALLSHIP_AI_H

#include <stdbool.h>
#include <stdint.h>

#define BOARD_W 8
#define BOARD_H 7
#define SHIP_COUNT 6
#define MAX_SHIP_LENGTH 3
#define INVALID_COORD UINT8_C(255)
#define WALLSHIP_DEFAULT_SEED UINT32_C(0x6D2B79F5)

/* Override at compile time to exercise bounded placement failure. */
#ifndef MAX_SHIP_PLACEMENT_ATTEMPTS
#define MAX_SHIP_PLACEMENT_ATTEMPTS 100
#endif
#ifndef MAX_FLEET_PLACEMENT_ATTEMPTS
#define MAX_FLEET_PLACEMENT_ATTEMPTS 10
#endif

typedef enum { SHIP_HORIZONTAL = 0, SHIP_VERTICAL } ShipOrientation;
typedef enum { CELL_UNKNOWN = 0, CELL_MISS, CELL_HIT, CELL_SUNK } CellState;
typedef enum { SHOT_MISS = 0, SHOT_HIT, SHOT_SUNK, SHOT_INVALID } ShotResult;
typedef enum { AI_RANDOM = 0, AI_BALANCED, AI_AGGRESSIVE } AIProfile;

typedef struct {
    uint8_t x;
    uint8_t y;
} Position;

typedef struct {
    uint8_t id;
    uint8_t length;
    uint8_t hp;
    bool sunk;
    uint8_t start_x;
    uint8_t start_y;
    ShipOrientation orientation;
} Ship;

typedef struct {
    CellState enemy_board[BOARD_H][BOARD_W];
    uint8_t own_board[BOARD_H][BOARD_W];
    uint8_t human_shot_board[BOARD_H][BOARD_W];
    Ship ships[SHIP_COUNT];
    uint8_t remaining_ship_1;
    uint8_t remaining_ship_2;
    uint8_t remaining_ship_3;
    AIProfile profile;
    uint32_t rng_state;
    Position last_shot;
    bool last_shot_valid;
    bool awaiting_sunk_details;
} WallshipAgent;

/* Core calls belong to one execution context (normally the main loop).
 * No function uses HAL, dynamic memory, floating point, or global RNG state.
 * NULL agents are tolerated: mutators do nothing, queries return failure.
 * Initialize before other calls. NewGame preserves the initialized profile.
 * Init/NewGame do NOT place ships. Check PlaceShips before starting play.
 */
void WallshipAI_Init(WallshipAgent *ai, uint32_t seed, AIProfile profile);
void WallshipAI_NewGame(WallshipAgent *ai, uint32_t seed);

/* Setup-only operation; clears own fleet and human shot history on each call.
 * On failure, leaves an empty board and zero-HP, non-sunk ship records.
 */
bool WallshipAI_PlaceShips(WallshipAgent *ai);

/* Reserves one unknown coordinate. Returns {255,255} while pending or exhausted.
 * Send a returned coordinate once. It stays UNKNOWN until valid feedback.
 */
Position WallshipAI_SelectShot(WallshipAgent *ai);

/* Only the outstanding coordinate is accepted. Invalid/mismatched feedback is
 * ignored, leaving it pending. SUNK records HIT until ReportSunkShip succeeds.
 */
void WallshipAI_ReportShotResult(WallshipAgent *ai, uint8_t x, uint8_t y,
                                ShotResult result);

/* Call AFTER reporting SHOT_SUNK. Coordinates may be in any order, but must
 * describe the complete straight contiguous ship, all currently HIT, including
 * the outstanding shot. Invalid/duplicate events have no effect. On acceptance
 * awaiting_sunk_details and last_shot_valid both become false.
 */
void WallshipAI_ReportSunkShip(WallshipAgent *ai, uint8_t ship_length,
                             const Position *ship_cells, uint8_t cell_count);

/* Repeats/out-of-range shots return INVALID without modifying HP or history.
 * Optional sunk_ship_length is set to zero except when returning SHOT_SUNK.
 */
ShotResult WallshipAI_ProcessHumanShot(WallshipAgent *ai, uint8_t x, uint8_t y,
                                     uint8_t *sunk_ship_length);
bool WallshipAI_ShouldContinueTurn(ShotResult result);

#endif
