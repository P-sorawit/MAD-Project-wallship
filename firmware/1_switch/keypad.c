/* STM32 side of module 1. CubeMX setup expected:
 *   COL0..COL7  : GPIO_Output, Open Drain, no pull (released = High-Z)
 *   ROW0..ROW27 : GPIO_Input, external 10k pull-up to 3.3 V, or GPIO Pull-up
 *                 in CubeMX instead (ROW0-6 panel 0, 7-13 panel 1 ...)
 *   TIM6        : update every 200 us (e.g. PSC 107, ARR 199 @ 108 MHz), NVIC on
 * Name the pins exactly COL0.., ROW0.. in CubeMX so main.h has the labels.
 * Diode anode on the row side, cathode on the column side: pull a column low,
 * and a pressed key pulls its row low.
 */
#include "main.h"
#include "keypad.h"
#include "app_config.h"

extern TIM_HandleTypeDef htim6;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} Pin;

#define PIN(name) { name##_GPIO_Port, name##_Pin }

static const Pin cols[KEYPAD_COLS] = {
    PIN(COL0), PIN(COL1), PIN(COL2), PIN(COL3),
    PIN(COL4), PIN(COL5), PIN(COL6), PIN(COL7)
};

static const Pin rows[KEYPAD_ROWS] = {
    PIN(ROW0),  PIN(ROW1),  PIN(ROW2),  PIN(ROW3),  PIN(ROW4),  PIN(ROW5),  PIN(ROW6),
    PIN(ROW7),  PIN(ROW8),  PIN(ROW9),  PIN(ROW10), PIN(ROW11), PIN(ROW12), PIN(ROW13),
    PIN(ROW14), PIN(ROW15), PIN(ROW16), PIN(ROW17), PIN(ROW18), PIN(ROW19), PIN(ROW20),
    PIN(ROW21), PIN(ROW22), PIN(ROW23), PIN(ROW24), PIN(ROW25), PIN(ROW26), PIN(ROW27)
};

static uint8_t active_col;

void Keypad_Init(void)
{
    uint8_t c;
    Keypad_ResetCore();
    for (c = 0; c < KEYPAD_COLS; ++c) {
        HAL_GPIO_WritePin(cols[c].port, cols[c].pin, GPIO_PIN_SET); /* High-Z */
    }
    active_col = 0;
    HAL_GPIO_WritePin(cols[0].port, cols[0].pin, GPIO_PIN_RESET);
    HAL_TIM_Base_Start_IT(&htim6);
}

void Keypad_ScanIsr(void)
{
    uint32_t pressed = 0;
    uint8_t r;
    /* Column was activated one tick (200 us) ago, so rows have settled. */
    for (r = 0; r < KEYPAD_ROWS; ++r) {
        if ((rows[r].port->IDR & rows[r].pin) == 0U) {
            pressed |= UINT32_C(1) << r;
        }
    }
    Keypad_ProcessColumn(active_col, pressed);

    HAL_GPIO_WritePin(cols[active_col].port, cols[active_col].pin, GPIO_PIN_SET);
    active_col = (uint8_t)((active_col + 1U) % KEYPAD_COLS);
    HAL_GPIO_WritePin(cols[active_col].port, cols[active_col].pin, GPIO_PIN_RESET);
}
