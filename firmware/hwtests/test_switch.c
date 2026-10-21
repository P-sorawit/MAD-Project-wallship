/* TEST 2: switch matrix (8 columns x 28 rows, TIM6 scan every 200 us).
 * Files: this + 1_switch/keypad.c, keypad_core.c
 *        (+ 2_led/ws2812.c, ws2812_port_stm32.c with TEST_SWITCH_WITH_LED 1)
 *
 * Watch (Live Expressions):
 *   test_key_count                total presses
 *   test_last_panel/x/y           last press: panel 0..3, x 0..7 = A..H, y 0..6 = row 1..7
 *   test_seen[panel]              distinct keys pressed per panel (target: 56)
 *   test_seen_map[panel][row]     bit x = key (x,row) has been pressed (0xFF = full row)
 *   keypad_debug_raw[col]         raw rows on each column right now, bit = ROWn
 * With TEST_SWITCH_WITH_LED 1 the LED under each pressed key toggles white
 * (last press red), each panel has its own dim background color.
 */
#include "main.h"
#include "hwtests.h"
#include "app_config.h"
#include "keypad.h"

#ifndef TEST_SWITCH_WITH_LED
#define TEST_SWITCH_WITH_LED 0
#endif

#if TEST_SWITCH_WITH_LED
#include "ws2812.h"
void Ws2812_Port_PulseFinished(TIM_HandleTypeDef *htim);
static bool lit[4][BOARD_W][BOARD_H];
static bool dirty = true;
#endif

volatile uint16_t test_key_count;
volatile uint8_t test_last_panel, test_last_x, test_last_y;
volatile uint8_t test_seen[4];
volatile uint8_t test_seen_map[4][BOARD_H];

void TestSwitch_Init(void)
{
#if TEST_SWITCH_WITH_LED
    Ws2812_Init();
#endif
    Keypad_Init();
}

void TestSwitch_Loop(void)
{
    KeyEvent e;
    while (Keypad_GetEvent(&e)) {
        const uint8_t bit = (uint8_t)(1U << e.x);
        ++test_key_count;
        test_last_panel = e.panel;
        test_last_x = e.x;
        test_last_y = e.y;
        if ((test_seen_map[e.panel][e.y] & bit) == 0U) {
            test_seen_map[e.panel][e.y] = (uint8_t)(test_seen_map[e.panel][e.y] | bit);
            ++test_seen[e.panel];
        }
#if TEST_SWITCH_WITH_LED
        lit[e.panel][e.x][e.y] = !lit[e.panel][e.x][e.y];
        dirty = true;
#endif
    }
#if TEST_SWITCH_WITH_LED
    if (dirty && !Ws2812_IsBusy()) {
        static const Rgb tint[4] = {{0, 0, 60}, {0, 50, 0}, {40, 0, 50}, {50, 40, 0}};
        static const Rgb on = {255, 255, 255}, last = {255, 0, 0};
        uint8_t p, x, y;
        for (p = 0; p < 4U; ++p) {
            for (x = 0; x < BOARD_W; ++x) {
                for (y = 0; y < BOARD_H; ++y) {
                    Ws2812_SetPixel(p, x, y, lit[p][x][y] ? on : tint[p]);
                }
            }
        }
        if (test_key_count != 0U) {
            Ws2812_SetPixel(test_last_panel, test_last_x, test_last_y, last);
        }
        Ws2812_Show();
        dirty = false;
    }
#endif
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        Keypad_ScanIsr();
    }
}

#if TEST_SWITCH_WITH_LED
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    Ws2812_Port_PulseFinished(htim);
}
#endif
