#include "wallship_ai.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    int32_t hunt;
    int32_t adjacent;
    int32_t line;
    uint32_t jitter;
} HeuristicConfig;

static const HeuristicConfig balanced_config = {10, 400, 1200, 5};
static const HeuristicConfig aggressive_config = {6, 800, 2000, 3};
static const uint8_t fleet_lengths[SHIP_COUNT] = {3, 3, 2, 2, 1, 1};

static bool is_inside_board(int x, int y)
{
    return x >= 0 && x < BOARD_W && y >= 0 && y < BOARD_H;
}

static Position invalid_position(void)
{
    const Position p = {INVALID_COORD, INVALID_COORD};
    return p;
}

static uint32_t xorshift32(WallshipAgent *ai)
{
    uint32_t x = ai->rng_state;
    if (x == 0) {
        x = WALLSHIP_DEFAULT_SEED;
    }
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    ai->rng_state = x;
    return x;
}

/* xorshift32 has UINT32_MAX nonzero outputs. Shift to [0, UINT32_MAX-1]
 * and discard the incomplete bucket to avoid modulo bias. bound must be > 0.
 */
static uint32_t random_below(WallshipAgent *ai, uint32_t bound)
{
    const uint32_t limit = UINT32_MAX - (UINT32_MAX % bound);
    uint32_t value;
    do {
        value = xorshift32(ai) - UINT32_C(1);
    } while (value >= limit);
    return value % bound;
}

static uint8_t *remaining_counter(WallshipAgent *ai, uint8_t length)
{
    switch (length) {
    case 1: return &ai->remaining_ship_1;
    case 2: return &ai->remaining_ship_2;
    case 3: return &ai->remaining_ship_3;
    default: return NULL;
    }
}

static void clear_own_board(WallshipAgent *ai)
{
    int i;
    memset(ai->own_board, 0, sizeof ai->own_board);
    memset(ai->human_shot_board, 0, sizeof ai->human_shot_board);
    memset(ai->ships, 0, sizeof ai->ships);
    for (i = 0; i < SHIP_COUNT; ++i) {
        ai->ships[i].id = (uint8_t)(i + 1);
        ai->ships[i].length = fleet_lengths[i];
    }
}

static void reset_game(WallshipAgent *ai, uint32_t seed)
{
    memset(ai->enemy_board, 0, sizeof ai->enemy_board);
    clear_own_board(ai);
    ai->remaining_ship_1 = 2;
    ai->remaining_ship_2 = 2;
    ai->remaining_ship_3 = 2;
    ai->rng_state = seed != 0 ? seed : WALLSHIP_DEFAULT_SEED;
    ai->last_shot = invalid_position();
    ai->last_shot_valid = false;
    ai->awaiting_sunk_details = false;
}

void WallshipAI_Init(WallshipAgent *ai, uint32_t seed, AIProfile profile)
{
    if (ai == NULL) {
        return;
    }
    memset(ai, 0, sizeof *ai);
    ai->profile = (profile >= AI_RANDOM && profile <= AI_AGGRESSIVE)
        ? profile : AI_BALANCED;
    reset_game(ai, seed);
}

void WallshipAI_NewGame(WallshipAgent *ai, uint32_t seed)
{
    if (ai != NULL) {
        reset_game(ai, seed);
    }
}

static bool is_valid_ship_position(const WallshipAgent *ai, int x, int y,
                                   uint8_t length, ShipOrientation orientation)
{
    const int dx = orientation == SHIP_HORIZONTAL ? 1 : 0;
    const int dy = orientation == SHIP_VERTICAL ? 1 : 0;
    int i;
    for (i = 0; i < length; ++i) {
        const int nx = x + dx * i;
        const int ny = y + dy * i;
        if (!is_inside_board(nx, ny) || ai->own_board[ny][nx] != 0) {
            return false;
        }
    }
    return true;
}

static bool place_single_ship(WallshipAgent *ai, Ship *ship)
{
    int attempt;
    for (attempt = 0; attempt < MAX_SHIP_PLACEMENT_ATTEMPTS; ++attempt) {
        const int x = (int)random_below(ai, BOARD_W);
        const int y = (int)random_below(ai, BOARD_H);
        const ShipOrientation orientation =
            (ShipOrientation)random_below(ai, 2);
        int i;
        if (!is_valid_ship_position(ai, x, y, ship->length, orientation)) {
            continue;
        }
        ship->start_x = (uint8_t)x;
        ship->start_y = (uint8_t)y;
        ship->orientation = ship->length == 1 ? SHIP_HORIZONTAL : orientation;
        ship->hp = ship->length;
        ship->sunk = false;
        for (i = 0; i < ship->length; ++i) {
            const int nx = x + (ship->orientation == SHIP_HORIZONTAL ? i : 0);
            const int ny = y + (ship->orientation == SHIP_VERTICAL ? i : 0);
            ai->own_board[ny][nx] = ship->id;
        }
        return true;
    }
    return false;
}

bool WallshipAI_PlaceShips(WallshipAgent *ai)
{
    int attempt;
    if (ai == NULL) {
        return false;
    }
    for (attempt = 0; attempt < MAX_FLEET_PLACEMENT_ATTEMPTS; ++attempt) {
        int i;
        clear_own_board(ai);
        for (i = 0; i < SHIP_COUNT; ++i) {
            if (!place_single_ship(ai, &ai->ships[i])) {
                break;
            }
        }
        if (i == SHIP_COUNT) {
            return true;
        }
    }
    clear_own_board(ai);
    return false;
}

/* A candidate is independent of adjacent HIT cells outside its own footprint.
 * This matters when several different ships touch one another.
 */
static bool candidate_hits(const WallshipAgent *ai, int x, int y, int dx, int dy,
                           int length, int *hits)
{
    int i;
    *hits = 0;
    for (i = 0; i < length; ++i) {
        const int nx = x + dx * i;
        const int ny = y + dy * i;
        CellState state;
        if (!is_inside_board(nx, ny)) {
            return false;
        }
        state = ai->enemy_board[ny][nx];
        if (state != CELL_UNKNOWN && state != CELL_HIT) {
            return false;
        }
        if (state == CELL_HIT) {
            ++*hits;
        }
    }
    return true;
}

/* Target level 0 = hunt, 1 = adjacent HIT, 2 = aligned HIT pair.
 * Store a maximum, never a sum, so many touching hits cannot inflate bonuses.
 */
static void calculate_scores(WallshipAgent *ai, const HeuristicConfig *config,
                             int32_t score[BOARD_H][BOARD_W])
{
    uint8_t target[BOARD_H][BOARD_W] = {{0}};
    int length, direction, x, y;
    memset(score, 0, sizeof(int32_t) * BOARD_H * BOARD_W);
    for (length = 1; length <= MAX_SHIP_LENGTH; ++length) {
        const uint8_t count = *remaining_counter(ai, (uint8_t)length);
        if (count == 0) {
            continue;
        }
        for (direction = 0; direction < (length == 1 ? 1 : 2); ++direction) {
            const int dx = direction == 0 ? 1 : 0;
            const int dy = direction == 1 ? 1 : 0;
            for (y = 0; y < BOARD_H; ++y) {
                for (x = 0; x < BOARD_W; ++x) {
                    int hits, i;
                    if (!candidate_hits(ai, x, y, dx, dy, length, &hits)) {
                        continue;
                    }
                    for (i = 0; i < length; ++i) {
                        const int nx = x + dx * i;
                        const int ny = y + dy * i;
                        uint8_t level = 0;
                        if (ai->enemy_board[ny][nx] != CELL_UNKNOWN) {
                            continue;
                        }
                        score[ny][nx] += config->hunt * count;
                        if (hits >= 2) {
                            level = 2;
                        } else if (hits == 1 &&
                            ((i > 0 && ai->enemy_board[ny-dy][nx-dx] == CELL_HIT) ||
                             (i+1 < length && ai->enemy_board[ny+dy][nx+dx] == CELL_HIT))) {
                            level = 1;
                        }
                        if (level > target[ny][nx]) {
                            target[ny][nx] = level;
                        }
                    }
                }
            }
        }
    }
    for (y = 0; y < BOARD_H; ++y) {
        for (x = 0; x < BOARD_W; ++x) {
            if (ai->enemy_board[y][x] == CELL_UNKNOWN) {
                score[y][x] += target[y][x] == 2 ? config->line
                    : (target[y][x] == 1 ? config->adjacent : 0);
                score[y][x] += (int32_t)random_below(ai, config->jitter + 1);
            }
        }
    }
}

static Position choose_random_unknown(WallshipAgent *ai)
{
    Position best = invalid_position();
    uint32_t count = 0;
    int x, y;
    for (y = 0; y < BOARD_H; ++y) {
        for (x = 0; x < BOARD_W; ++x) {
            if (ai->enemy_board[y][x] == CELL_UNKNOWN) {
                ++count;
                if (random_below(ai, count) == 0) {
                    best.x = (uint8_t)x;
                    best.y = (uint8_t)y;
                }
            }
        }
    }
    return best;
}

static Position choose_highest_score(WallshipAgent *ai,
                                     int32_t score[BOARD_H][BOARD_W])
{
    Position best = invalid_position();
    int32_t best_score = INT32_MIN;
    uint32_t ties = 0;
    int x, y;
    for (y = 0; y < BOARD_H; ++y) {
        for (x = 0; x < BOARD_W; ++x) {
            if (ai->enemy_board[y][x] != CELL_UNKNOWN) {
                continue;
            }
            if (score[y][x] > best_score) {
                best_score = score[y][x];
                ties = 1;
                best.x = (uint8_t)x;
                best.y = (uint8_t)y;
            } else if (score[y][x] == best_score) {
                ++ties;
                if (random_below(ai, ties) == 0) {
                    best.x = (uint8_t)x;
                    best.y = (uint8_t)y;
                }
            }
        }
    }
    return best;
}

Position WallshipAI_SelectShot(WallshipAgent *ai)
{
    Position selected;
    if (ai == NULL || ai->last_shot_valid || ai->awaiting_sunk_details) {
        return invalid_position();
    }
    if (ai->profile == AI_RANDOM) {
        selected = choose_random_unknown(ai);
    } else {
        int32_t score[BOARD_H][BOARD_W];
        const HeuristicConfig *config = ai->profile == AI_AGGRESSIVE
            ? &aggressive_config : &balanced_config;
        calculate_scores(ai, config, score);
        selected = choose_highest_score(ai, score);
    }
    ai->last_shot = selected;
    ai->last_shot_valid = is_inside_board(selected.x, selected.y);
    return selected;
}

void WallshipAI_ReportShotResult(WallshipAgent *ai, uint8_t x, uint8_t y,
                                ShotResult result)
{
    if (ai == NULL || !is_inside_board(x, y) || !ai->last_shot_valid ||
        ai->awaiting_sunk_details || ai->last_shot.x != x || ai->last_shot.y != y ||
        ai->enemy_board[y][x] != CELL_UNKNOWN ||
        (result != SHOT_MISS && result != SHOT_HIT && result != SHOT_SUNK)) {
        return;
    }
    ai->enemy_board[y][x] = result == SHOT_MISS ? CELL_MISS : CELL_HIT;
    ai->awaiting_sunk_details = result == SHOT_SUNK;
    ai->last_shot_valid = ai->awaiting_sunk_details;
}

void WallshipAI_ReportSunkShip(WallshipAgent *ai, uint8_t ship_length,
                             const Position *ship_cells, uint8_t cell_count)
{
    uint8_t *remaining;
    int i, j;
    int min_x = BOARD_W, min_y = BOARD_H, max_x = -1, max_y = -1;
    bool includes_last = false;
    if (ai == NULL || ship_cells == NULL || !ai->last_shot_valid ||
        !ai->awaiting_sunk_details || ship_length < 1 ||
        ship_length > MAX_SHIP_LENGTH || cell_count != ship_length) {
        return;
    }
    remaining = remaining_counter(ai, ship_length);
    if (*remaining == 0) {
        return;
    }
    for (i = 0; i < cell_count; ++i) {
        const int x = ship_cells[i].x;
        const int y = ship_cells[i].y;
        if (!is_inside_board(x, y) || ai->enemy_board[y][x] != CELL_HIT) {
            return;
        }
        for (j = 0; j < i; ++j) {
            if (ship_cells[j].x == x && ship_cells[j].y == y) {
                return;
            }
        }
        if (ai->last_shot.x == x && ai->last_shot.y == y) {
            includes_last = true;
        }
        if (x < min_x) { min_x = x; }
        if (x > max_x) { max_x = x; }
        if (y < min_y) { min_y = y; }
        if (y > max_y) { max_y = y; }
    }
    if (!includes_last ||
        !((min_y == max_y && max_x-min_x+1 == ship_length) ||
          (min_x == max_x && max_y-min_y+1 == ship_length))) {
        return;
    }
    for (i = 0; i < cell_count; ++i) {
        ai->enemy_board[ship_cells[i].y][ship_cells[i].x] = CELL_SUNK;
    }
    --*remaining;
    ai->last_shot_valid = false;
    ai->awaiting_sunk_details = false;
}

ShotResult WallshipAI_ProcessHumanShot(WallshipAgent *ai, uint8_t x, uint8_t y,
                                     uint8_t *sunk_ship_length)
{
    uint8_t id;
    Ship *ship;
    if (sunk_ship_length != NULL) {
        *sunk_ship_length = 0;
    }
    if (ai == NULL || !is_inside_board(x, y) || ai->human_shot_board[y][x] != 0) {
        return SHOT_INVALID;
    }
    id = ai->own_board[y][x];
    if (id > SHIP_COUNT) {
        return SHOT_INVALID;
    }
    if (id == 0) {
        ai->human_shot_board[y][x] = 1;
        return SHOT_MISS;
    }
    ship = &ai->ships[id-1];
    if (ship->id != id || ship->hp == 0 || ship->sunk) {
        return SHOT_INVALID;
    }
    ai->human_shot_board[y][x] = 1;
    --ship->hp;
    if (ship->hp != 0) {
        return SHOT_HIT;
    }
    ship->sunk = true;
    if (sunk_ship_length != NULL) {
        *sunk_ship_length = ship->length;
    }
    return SHOT_SUNK;
}

bool WallshipAI_ShouldContinueTurn(ShotResult result)
{
    return result == SHOT_HIT || result == SHOT_SUNK;
}
