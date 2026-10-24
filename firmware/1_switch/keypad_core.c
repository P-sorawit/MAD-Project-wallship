/* HAL-free part of module 1: debounce + event queue. */
#include "keypad.h"

#include "app_config.h"

#include <string.h>

static uint8_t counter[KEYPAD_COLS][KEYPAD_ROWS]; /* consecutive equal scans */
static uint32_t stable[KEYPAD_COLS];              /* debounced state, bit = row */
volatile uint32_t keypad_debug_raw[KEYPAD_COLS];   /* last raw scan, for debugging */
volatile uint16_t keypad_ghost_drops;              /* presses dropped by maybe_ghost() */

static KeyEvent queue[KEYPAD_QUEUE_SIZE];
static volatile uint8_t q_head; /* written by ISR */
static volatile uint8_t q_tail; /* written by main loop */

static const uint8_t group_to_panel[4] = KEYPAD_GROUP_TO_PANEL;
static const uint8_t flip_x[4] = KEYPAD_FLIP_X;
static const uint8_t flip_y[4] = KEYPAD_FLIP_Y;

void Keypad_ResetCore(void)
{
    uint8_t c;
    memset(counter, 0, sizeof counter);
    memset(stable, 0, sizeof stable);
    for (c = 0; c < KEYPAD_COLS; ++c) {
        keypad_debug_raw[c] = 0;
    }
    keypad_ghost_drops = 0;
    q_head = 0;
    q_tail = 0;
}

/* With a missing, reversed or shorted diode, three held keys in an L make
 * the fourth corner of the rectangle read as pressed too. So a key whose
 * column and row both share held keys with one other column cannot be
 * trusted: drop it rather than report a cell nobody pressed. */
static bool maybe_ghost(uint8_t col, uint8_t row)
{
    const uint32_t bit = UINT32_C(1) << row;
    uint8_t c;
    for (c = 0; c < KEYPAD_COLS; ++c) {
        if (c != col && (keypad_debug_raw[c] & bit) != 0U &&
            (keypad_debug_raw[c] & keypad_debug_raw[col] & ~bit) != 0U) {
            return true;
        }
    }
    return false;
}

static void push_press(uint8_t col, uint8_t row)
{
    const uint8_t next = (uint8_t)((q_head + 1U) & (KEYPAD_QUEUE_SIZE - 1U));
    const uint8_t panel = group_to_panel[row / BOARD_H];
    KeyEvent *e;
    if (next == q_tail) {
        return; /* queue full: drop the press */
    }
    e = &queue[q_head];
    e->panel = panel;
    e->x = flip_x[panel] ? (uint8_t)(BOARD_W - 1U - col) : col;
    e->y = flip_y[panel] ? (uint8_t)(BOARD_H - 1U - row % BOARD_H)
                         : (uint8_t)(row % BOARD_H);
    q_head = next;
}

void Keypad_ProcessColumn(uint8_t col, uint32_t pressed_rows)
{
    uint8_t row;
    if (col >= KEYPAD_COLS) {
        return;
    }
    keypad_debug_raw[col] = pressed_rows;
    for (row = 0; row < KEYPAD_ROWS; ++row) {
        const uint32_t bit = UINT32_C(1) << row;
        const bool now = (pressed_rows & bit) != 0;
        const bool was = (stable[col] & bit) != 0;
        if (now == was) {
            counter[col][row] = 0;
            continue;
        }
        if (++counter[col][row] >= KEYPAD_DEBOUNCE_SCANS) {
            counter[col][row] = 0;
            stable[col] ^= bit;
            if (now) {                /* report press edge only */
                if (maybe_ghost(col, row)) {
                    ++keypad_ghost_drops;
                } else {
                    push_press(col, row);
                }
            }
        }
    }
}

bool Keypad_GetEvent(KeyEvent *event)
{
    if (q_tail == q_head) {
        return false;
    }
    *event = queue[q_tail];
    q_tail = (uint8_t)((q_tail + 1U) & (KEYPAD_QUEUE_SIZE - 1U));
    return true;
}
