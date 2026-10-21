/* TEST 6: ILI9341 x2 on SPI1 + DMA (no touch).
 * Files: this + 4_tft/tft.c, gfx.c, font5x7.c
 * Each display shows:
 *   - color bars R G B W from the left       (R/B swapped -> MADCTL BGR bit)
 *   - "TOP LEFT" in the top-left corner      (mirrored/rotated -> MADCTL)
 *   - a 1-pixel white frame on all 4 edges   (an edge missing -> MADCTL / window)
 *   - "TFT 1" / "TFT 2"                      (wrong display -> CS/CS2 swapped)
 *   - a counter updating every 250 ms, sent as a partial (band) update
 *   - measured times: full frame ms and counter-band ms
 *
 * First, as a basic check, each display is filled solid RED -> GREEN -> BLUE
 * with plain blocking SPI (no DMA, no framebuffer). If the screen stays white
 * even then, the problem is wiring/power, not DMA or drawing code.
 *
 * Watch: test_tft_stage (1 init sent, 2 solid fills sent, 3 DMA frame done,
 *        4 running), test_tft_full_ms[2], test_tft_band_ms, test_tft_updates
 * At SPI 27 MHz expect ~45 ms full / ~5 ms band; 13.5 MHz ~91 / ~10.
 */
#include "main.h"
#include "hwtests.h"
#include "gfx.h"
#include "tft.h"

#include <stdio.h>

#define COUNTER_Y   (9 * GFX_BAND_H)      /* rows 144..175 = bands 9 and 10 */
#define COUNTER_MASK ((1UL << 9) | (1UL << 10))

volatile uint8_t test_tft_stage;
volatile uint32_t test_tft_full_ms[2];
volatile uint32_t test_tft_band_ms;
volatile uint32_t test_tft_updates;

static uint32_t next_ms, sent_ms;
static uint8_t next_display;
static bool measuring;

static void draw_counter(uint8_t d)
{
    char buf[32];
    Gfx_Rect(1, COUNTER_Y, GFX_W - 2, 2 * GFX_BAND_H, 0x0C2150U);
    snprintf(buf, sizeof buf, "UPDATE %lu", (unsigned long)(test_tft_updates % 100000UL));
    Gfx_TextCenter(GFX_W / 2, COUNTER_Y + 4, buf, 2, 1, 0xFFFFFFU);
    snprintf(buf, sizeof buf, "FULL %lu MS  BAND %lu MS", (unsigned long)test_tft_full_ms[d],
             (unsigned long)test_tft_band_ms);
    Gfx_TextCenter(GFX_W / 2, COUNTER_Y + 20, buf, 1, 1, 0x7FD6FFU);
}

static void draw_full(uint8_t d)
{
    char buf[16];
    Gfx_Rect(0, 0, GFX_W, GFX_H, 0x000000U);
    Gfx_Rect(0, 0, 60, 24, 0xFF0000U);
    Gfx_Rect(60, 0, 60, 24, 0x00FF00U);
    Gfx_Rect(120, 0, 60, 24, 0x0000FFU);
    Gfx_Rect(180, 0, 60, 24, 0xFFFFFFU);
    Gfx_Text(26, 9, "R", 1, 0, 0xFFFFFFU);
    Gfx_Text(86, 9, "G", 1, 0, 0x000000U);
    Gfx_Text(146, 9, "B", 1, 0, 0xFFFFFFU);
    Gfx_Text(206, 9, "W", 1, 0, 0x000000U);
    Gfx_Text(4, 30, "TOP LEFT", 2, 1, 0xFFE14AU);
    Gfx_Text(GFX_W - 4 - Gfx_TextWidth("BOTTOM RIGHT", 1, 1), GFX_H - 12, "BOTTOM RIGHT", 1, 1,
             0xFFE14AU);
    snprintf(buf, sizeof buf, "TFT %u", (unsigned)d + 1U);
    Gfx_TitleCenter(GFX_W / 2, 70, buf, 5, 2, 0xFFF27AU, 0xFF8A1EU, 0x0B1A44U, 2);
    Gfx_VGradient(10, 200, GFX_W - 20, 90, 0x000000U, 0xFFFFFFU, 16); /* gray steps */
    Gfx_Frame(0, 0, GFX_W, GFX_H, 1, 0xFFFFFFU);
    draw_counter(d);
}

void TestTft_Init(void)
{
    static const Color solid[3] = {0xFF0000U, 0x00FF00U, 0x0000FFU};
    uint8_t d;
    int c;
    Tft_Init(0);
    Tft_Init(1);
    test_tft_stage = 1;
    for (c = 0; c < 3; ++c) {               /* basic check: no DMA involved */
        Tft_FillBlocking(0, Gfx_To565(solid[c]));
        Tft_FillBlocking(1, Gfx_To565(solid[c]));
        HAL_Delay(700);
    }
    test_tft_stage = 2;
    for (d = 0; d < 2U; ++d) {
        uint32_t t0;
        draw_full(d);
        t0 = HAL_GetTick();
        Tft_SendBands(d, (1UL << GFX_BANDS) - 1UL);
        while (Tft_IsBusy()) {
        }
        test_tft_full_ms[d] = HAL_GetTick() - t0;
    }
    test_tft_stage = 3;
    next_ms = HAL_GetTick();
}

void TestTft_Loop(void)
{
    const uint32_t now = HAL_GetTick();
    if (measuring && !Tft_IsBusy()) {
        test_tft_band_ms = now - sent_ms;
        measuring = false;
    }
    test_tft_stage = 4;
    if (Tft_IsBusy() || (int32_t)(now - next_ms) < 0) {
        return;
    }
    next_ms = now + 250U;
    ++test_tft_updates;
    draw_counter(next_display);
    sent_ms = now;
    measuring = true;
    Tft_SendBands(next_display, COUNTER_MASK);   /* only 2 of 20 bands */
    next_display = (uint8_t)(1U - next_display);
}
