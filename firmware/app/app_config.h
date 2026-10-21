/* Hardware wiring and tuning values in one place.
 * Anything marked TODO must be checked against the real wiring.
 * Panel numbers: 0 = P1 fleet, 1 = P1 attack, 2 = P2 fleet, 3 = P2 attack.
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "wallship_types.h" /* BOARD_W = 8 (A-H), BOARD_H = 7 (1-7) */

/* 1 = hardware bring-up test (app/hwtest.c, BRINGUP.md) instead of the game. */
#ifndef APP_HW_TEST
#define APP_HW_TEST            0
#endif

/* ---------- Module 1: switch matrix (TIM6, 200 us per column) ---------- */
#define KEYPAD_COLS            8U
#define KEYPAD_ROWS            28U   /* 7 rows x 4 panels */
#define KEYPAD_DEBOUNCE_SCANS  4U    /* 4 full scans x 1.6 ms = 6.4 ms stable */
#define KEYPAD_QUEUE_SIZE      16U   /* power of two */

/* Row group g (rows 7g..7g+6) belongs to which panel. TODO: check wiring. */
#define KEYPAD_GROUP_TO_PANEL  { 0U, 1U, 2U, 3U }
/* Mirror a panel if its switch columns/rows are wired right-to-left or
 * bottom-to-top (e.g. the attack panels on the divider). TODO: check. */
#define KEYPAD_FLIP_X          { 0U, 0U, 0U, 0U }
#define KEYPAD_FLIP_Y          { 0U, 0U, 0U, 0U }

/* ---------- Module 2: WS2812B (TIM1 CH1-CH4 + DMA2) ---------- */
#define WS2812_CHANNELS        4U
#define WS2812_CCR_0           86U   /* 0.40 us high @ 216 MHz, ARR = 269 */
#define WS2812_CCR_1           173U  /* 0.80 us high */
/* Which TIM1 channels are wired AND have a DMA stream in CubeMX.
 * bit0 = CH1 ... bit3 = CH4. Start with 0x01 to test a single strip;
 * starting a channel without DMA configured would crash in HAL. */
#define WS2812_ACTIVE_MASK     0x0FU
/* TIM1 CH(n+1) drives which panel. TODO: check wiring. */
#define WS2812_CHANNEL_TO_PANEL { 0U, 1U, 2U, 3U }
/* Global brightness cap: 102/255 = 40% (keeps 224 LEDs under 10 A). */
#define WS2812_BRIGHTNESS_CAP  102U
/* LED chain order inside a panel: index = y * 8 + x starting at (A,1).
 * Set to 1 if the pixel string snakes (odd rows run right-to-left). */
#define WS2812_SERPENTINE      0
#define WS2812_FLIP_X          { 0U, 0U, 0U, 0U }
#define WS2812_FLIP_Y          { 0U, 0U, 0U, 0U }

/* ---------- Module 4: touch panels (XPT2046) ---------- */
/* Raw 12-bit readings at the screen edges, per panel { P1, P2 }.
 * TODO: calibrate on hardware (see 4_tft/touch.c). min > max is allowed
 * and flips that axis. */
#define TOUCH_RAW_X_MIN        { 300U, 300U }
#define TOUCH_RAW_X_MAX        { 3800U, 3800U }
#define TOUCH_RAW_Y_MIN        { 300U, 300U }
#define TOUCH_RAW_Y_MAX        { 3800U, 3800U }
#define TOUCH_SWAP_XY          { 0U, 0U }

/* ---------- Module 5: LCD1602 via PCF8574 (I2C1) ---------- */
#define LCD_COUNT              2U
#define LCD_I2C_ADDR           { (0x27U << 1), (0x26U << 1) } /* P1, P2 (HAL wants 8-bit) */

/* ---------- Module 6: AI ---------- */
#define GAME_AI_PROFILE        AI_BALANCED /* AI_RANDOM / AI_BALANCED / AI_AGGRESSIVE */

#endif
