/* Module 1: 224-key switch matrix (8 shared columns x 28 rows, 1N4148 per key).
 * TIM6 interrupts every 200 us; each interrupt reads the 28 rows of the
 * active column, debounces, then activates the next column (full scan 1.6 ms).
 * Presses are queued; the main loop reads them with Keypad_GetEvent().
 */
#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t panel; /* 0 P1 fleet, 1 P1 attack, 2 P2 fleet, 3 P2 attack */
    uint8_t x;     /* 0..7 = column A..H */
    uint8_t y;     /* 0..6 = row 1..7 */
} KeyEvent;

void Keypad_Init(void);
void Keypad_ScanIsr(void);              /* call from TIM6 update interrupt */
bool Keypad_GetEvent(KeyEvent *event);  /* main loop; false if queue empty */

/* HAL-free core, shared by Keypad_ScanIsr and host tests:
 * feed the 28-bit row snapshot of one column (bit = 1 means pressed). */
void Keypad_ProcessColumn(uint8_t col, uint32_t pressed_rows);
void Keypad_ResetCore(void);

#endif
