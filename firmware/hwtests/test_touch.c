/* TEST 7: XPT2046 touch x2 - calibration, then a concurrency/latency test
 * while the displays are kept busy (needs the TFT working: test 6).
 * Files: this + 4_tft/touch.c, tft.c, gfx.c, font5x7.c (+ app_config.h)
 *
 * Step 1 - calibrate (each screen on its own):
 *   tap the 4 numbered targets in order 1 TL, 2 TR, 3 BR, 4 BL. The screen
 *   then shows X_MIN/X_MAX/Y_MIN/Y_MAX/SWAP_XY to copy into app_config.h.
 * Step 2 - test (starts right after step 1):
 *   a bar keeps moving at the bottom of both screens (DMA updates all the
 *   time, like the game redrawing). Tap anywhere: a red cross is drawn where
 *   the CURRENT config maps the tap, and the screen shows
 *     READ  ms : pen-down interrupt -> coordinates read
 *     SHOW  ms : pen-down interrupt -> cross visible on screen (DMA done)
 *     WORST ms : slowest SHOW so far
 *     LOOP  ms : longest gap between two main-loop passes
 *   Two people can tap both screens at the same time.
 *
 * Watch: test_touch_raw[2][2], test_touch_xy[2][2], test_touch_count[2],
 *        test_cal[2][5] = {X_MIN, X_MAX, Y_MIN, Y_MAX, SWAP},
 *        test_read_ms[2], test_show_ms[2], test_show_worst_ms[2], test_loop_max_ms
 */
#include "main.h"
#include "hwtests.h"
#include "gfx.h"
#include "tft.h"
#include "touch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void App_TouchIrq(uint8_t panel);   /* defined in touch.c */

#define INSET 20
#define BAR_Y (18 * GFX_BAND_H)     /* moving bar lives in band 18 */
static const int target[4][2] = {
    {INSET, INSET}, {GFX_W - 1 - INSET, INSET},
    {GFX_W - 1 - INSET, GFX_H - 1 - INSET}, {INSET, GFX_H - 1 - INSET}
};

volatile uint16_t test_touch_raw[2][2];
volatile int test_touch_xy[2][2];
volatile uint16_t test_touch_count[2];
volatile int32_t test_cal[2][5];
volatile uint32_t test_read_ms[2], test_show_ms[2], test_show_worst_ms[2];
volatile uint32_t test_loop_max_ms;

static uint16_t tap_raw[2][4][2];
static uint8_t step[2];                 /* 0..3 = next target, 4 = test mode */
static bool dirty[2] = {true, true};
static volatile uint32_t irq_ms[2];     /* set in the EXTI interrupt */
static uint32_t tap_ms[2];              /* irq time of a tap not yet on screen */
static bool tap_pending[2];
static uint32_t band_hash[2][GFX_BANDS];
static bool hash_ok[2];
static uint8_t sent_display;
static bool sent_has_tap;
static uint32_t sent_tap_ms;
static bool waiting_dma;
static uint8_t anim_display;
static uint32_t last_loop_ms;

static int32_t clamp12(int32_t v)
{
    return v < 0 ? 0 : (v > 4095 ? 4095 : v);
}

/* Fit the screen edges from the 4 inset targets. */
static void calibrate(uint8_t p)
{
    const uint16_t (*r)[2] = tap_raw[p];
    const int dx_rawx = abs((int)r[1][0] - (int)r[0][0]);
    const int dx_rawy = abs((int)r[1][1] - (int)r[0][1]);
    const int swap = dx_rawy > dx_rawx;              /* raw X runs vertically? */
    const int ax = swap ? 1 : 0, ay = swap ? 0 : 1;
    const int32_t left = ((int32_t)r[0][ax] + r[3][ax]) / 2, right = ((int32_t)r[1][ax] + r[2][ax]) / 2;
    const int32_t top = ((int32_t)r[0][ay] + r[1][ay]) / 2, bottom = ((int32_t)r[3][ay] + r[2][ay]) / 2;
    const int32_t span_x = target[1][0] - target[0][0], span_y = target[3][1] - target[0][1];
    test_cal[p][0] = clamp12(left - (right - left) * INSET / span_x);
    test_cal[p][1] = clamp12(right + (right - left) * INSET / span_x);
    test_cal[p][2] = clamp12(top - (bottom - top) * INSET / span_y);
    test_cal[p][3] = clamp12(bottom + (bottom - top) * INSET / span_y);
    test_cal[p][4] = swap;
}

static void draw(uint8_t p, uint32_t now)
{
    char buf[40];
    int i, y;
    Gfx_Rect(0, 0, GFX_W, GFX_H, 0x0C2150U);
    snprintf(buf, sizeof buf, "TOUCH P%u", (unsigned)p + 1U);
    Gfx_TextCenter(GFX_W / 2, 40, buf, 3, 2, 0xFFE14AU);

    if (step[p] < 4U) {
        for (i = 0; i < 4; ++i) {
            const Color c = step[p] == i ? 0xFF3A2EU : (step[p] > i ? 0x2DB84AU : 0x6E7682U);
            Gfx_Ring(target[i][0], target[i][1], 10, 2, c);
            Gfx_Rect(target[i][0] - 14, target[i][1], 29, 1, c);
            Gfx_Rect(target[i][0], target[i][1] - 14, 1, 29, c);
            snprintf(buf, sizeof buf, "%d", i + 1);
            Gfx_Text(target[i][0] + (i == 1 || i == 2 ? -26 : 16), target[i][1] + (i >= 2 ? -22 : 14),
                     buf, 2, 0, c);
        }
        snprintf(buf, sizeof buf, "CALIBRATE: TAP %u", (unsigned)step[p] + 1U);
        Gfx_TextCenter(GFX_W / 2, 90, buf, 2, 1, 0xFFFFFFU);
    } else {
        Gfx_TextCenter(GFX_W / 2, 74, "COPY TO APP_CONFIG.H", 1, 1, 0x9CFF8AU);
        snprintf(buf, sizeof buf, "X %4ld..%4ld  Y %4ld..%4ld  SW %ld", (long)test_cal[p][0],
                 (long)test_cal[p][1], (long)test_cal[p][2], (long)test_cal[p][3], (long)test_cal[p][4]);
        Gfx_TextCenter(GFX_W / 2, 86, buf, 1, 1, 0xFFFFFFU);
        Gfx_TextCenter(GFX_W / 2, 104, "TAP ANYWHERE", 2, 1, 0x7FD6FFU);
        y = 130;
        snprintf(buf, sizeof buf, "TAPS  %u", (unsigned)(test_touch_count[p] % 10000U));
        Gfx_Text(30, y, buf, 2, 1, 0xFFFFFFU);
        snprintf(buf, sizeof buf, "READ  %lu MS", (unsigned long)test_read_ms[p]);
        Gfx_Text(30, y + 20, buf, 2, 1, 0xFFFFFFU);
        snprintf(buf, sizeof buf, "SHOW  %lu MS", (unsigned long)test_show_ms[p]);
        Gfx_Text(30, y + 40, buf, 2, 1, test_show_ms[p] <= 50U ? 0x9CFF8AU : 0xFF9A2EU);
        snprintf(buf, sizeof buf, "WORST %lu MS", (unsigned long)test_show_worst_ms[p]);
        Gfx_Text(30, y + 60, buf, 2, 1, test_show_worst_ms[p] <= 50U ? 0x9CFF8AU : 0xFF9A2EU);
        snprintf(buf, sizeof buf, "LOOP  %lu MS", (unsigned long)test_loop_max_ms);
        Gfx_Text(30, y + 80, buf, 2, 1, 0xFFFFFFU);
    }
    snprintf(buf, sizeof buf, "RAW %4u %4u  XY %3d %3d", (unsigned)test_touch_raw[p][0],
             (unsigned)test_touch_raw[p][1], test_touch_xy[p][0], test_touch_xy[p][1]);
    Gfx_TextCenter(GFX_W / 2, 250, buf, 1, 1, 0x7FD6FFU);

    /* moving bar: keeps DMA busy like the game redrawing its screens */
    Gfx_Rect(1, BAR_Y, GFX_W - 2, GFX_BAND_H, 0x15346FU);
    Gfx_Rect((int)((now / 8U) % (GFX_W - 40)), BAR_Y + 3, 40, GFX_BAND_H - 6, 0xFF9A2EU);

    if (test_touch_count[p] != 0U) {
        const int x = test_touch_xy[p][0], yy = test_touch_xy[p][1];
        Gfx_Rect(x - 10, yy, 21, 2, 0xFF3A2EU);
        Gfx_Rect(x, yy - 10, 2, 21, 0xFF3A2EU);
    }
}

/* Draw display p and send only the bands that changed on it. */
static void refresh(uint8_t p, uint32_t now)
{
    uint32_t mask = 0;
    int b;
    draw(p, now);
    for (b = 0; b < GFX_BANDS; ++b) {
        const uint32_t h = Gfx_BandHash(b);
        if (!hash_ok[p] || h != band_hash[p][b]) {
            band_hash[p][b] = h;
            mask |= 1UL << b;
        }
    }
    hash_ok[p] = true;
    sent_display = p;
    sent_has_tap = tap_pending[p];
    sent_tap_ms = tap_ms[p];
    tap_pending[p] = false;
    waiting_dma = true;
    Tft_SendBands(p, mask);
}

void TestTouch_Init(void)
{
    Tft_Init(0);
    Tft_Init(1);
    Touch_Init();
    last_loop_ms = HAL_GetTick();
}

void TestTouch_Loop(void)
{
    const uint32_t now = HAL_GetTick();
    uint8_t p;

    if (now - last_loop_ms > test_loop_max_ms) {
        test_loop_max_ms = now - last_loop_ms;   /* how long the loop was blocked */
    }
    last_loop_ms = now;

    /* a frame finished: if it carried a tap, that is the tap-to-screen time */
    if (waiting_dma && !Tft_IsBusy()) {
        waiting_dma = false;
        if (sent_has_tap) {
            const uint8_t s = sent_display;
            test_show_ms[s] = now - sent_tap_ms;
            if (test_show_ms[s] > test_show_worst_ms[s]) {
                test_show_worst_ms[s] = test_show_ms[s];
            }
            dirty[s] = true;                     /* redraw with the new numbers */
        }
    }

    for (p = 0; p < 2U; ++p) {
        int x, y;
        if (Touch_Poll(p, &x, &y)) {
            uint16_t rx, ry;
            Touch_LastRaw(p, &rx, &ry);
            test_touch_raw[p][0] = rx;
            test_touch_raw[p][1] = ry;
            test_touch_xy[p][0] = x;
            test_touch_xy[p][1] = y;
            ++test_touch_count[p];
            test_read_ms[p] = HAL_GetTick() - irq_ms[p];
            if (step[p] < 4U) {
                tap_raw[p][step[p]][0] = rx;
                tap_raw[p][step[p]][1] = ry;
                if (++step[p] == 4U) {
                    calibrate(p);
                }
            } else {
                tap_pending[p] = true;
                tap_ms[p] = irq_ms[p];
            }
            dirty[p] = true;
        }
    }

    if (!Tft_IsBusy()) {
        /* taps first, otherwise animate the two screens in turn */
        if (dirty[0] || dirty[1]) {
            p = dirty[0] ? 0U : 1U;
            if (dirty[0] && dirty[1] && sent_display == 0U) {
                p = 1U;                          /* both waiting: alternate */
            }
            dirty[p] = false;
        } else {
            p = anim_display;
            anim_display = (uint8_t)(1U - anim_display);
        }
        refresh(p, now);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin == T_IRQ_Pin) {
        irq_ms[0] = HAL_GetTick();
        App_TouchIrq(0);
    } else if (pin == T_IRQ2_Pin) {
        irq_ms[1] = HAL_GetTick();
        App_TouchIrq(1);
    }
}
