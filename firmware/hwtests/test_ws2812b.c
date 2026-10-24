/* TEST 1b: WS2812B strips, stand-alone (no other driver file needed).
 * Use this to check a strip or panel before the game library (test_led.c).
 *
 * CubeMX: TIM1 CH1..CH4 "PWM Generation CHx" (only the ones you wire),
 *         DMA2 for each TIM1_CHx: Memory To Peripheral, Normal, Half Word / Half Word.
 *         Timer period/pulse values do not matter: they are set here from the real
 *         TIM1 clock (216 MHz -> 1.25 us = 270 ticks).
 * Channels without a DMA stream in CubeMX are skipped automatically.
 *
 * Every LED, cycling (about 17 s per round):
 *   1 FILL     red -> green -> blue -> white, 1 s each   (colour order, power)
 *   2 CHANNEL  CH1 red, CH2 green, CH3 blue, CH4 yellow  (which strip is which)
 *   3 MARKERS  LED 0 green, every WS_MARK_EVERY-th blue, last LED red
 *                                                        (count + direction)
 *   4 CHASE    one white LED walks from LED 0 to the end (broken link = it stops)
 *   5 RAINBOW  moving rainbow                            (flicker = timing/power)
 *
 * Live Expressions: test_ws_pattern, test_ws_frames, test_ws_errors,
 *                   test_ws_channels, test_ws_timclk
 */
#include "main.h"
#include "hwtests.h"
#include <string.h>

/* ---------------- settings ---------------- */
#define WS_LEDS         56U   /* LEDs on each channel (8x7 panel = 56) */
#define WS_MARK_EVERY   8U    /* MARKERS: blue every n LEDs (8 = start of each panel row) */
#define WS_BRIGHTNESS   64U   /* 0..255. 64 = 25 %: enough to test, light on the PSU */
#define WS_RESET_SLOTS  240U  /* 240 x 1.25 us = 300 us low after each frame */
#define WS_FRAME_MS     20U   /* refresh period (50 fps) */

extern TIM_HandleTypeDef htim1;

volatile uint8_t  test_ws_channels; /* bit n = TIM1 CH(n+1) has DMA, will be driven */
volatile uint32_t test_ws_timclk;   /* TIM1 clock, Hz. Expect 216000000 */
volatile uint8_t  test_ws_pattern;  /* 1..5, see top of file */
volatile uint32_t test_ws_frames;   /* frames fully sent. Stays 0 = DMA interrupt missing */
volatile uint32_t test_ws_errors;   /* HAL_TIM_PWM_Start_DMA refused */

#define WS_SLOTS (WS_LEDS * 24U + WS_RESET_SLOTS)

static uint16_t dma_buf[4][WS_SLOTS] __attribute__((aligned(32)));
static volatile uint8_t busy;       /* bit n = channel n still sending */
static uint16_t ccr_0, ccr_1;       /* compare value for a 0 bit / 1 bit */
static uint32_t last_frame_ms;

static const uint32_t tim_channel[4] = { TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4 };
static const uint16_t dma_id[4] = { TIM_DMA_ID_CC1, TIM_DMA_ID_CC2, TIM_DMA_ID_CC3, TIM_DMA_ID_CC4 };
static const HAL_TIM_ActiveChannel active[4] = {
    HAL_TIM_ACTIVE_CHANNEL_1, HAL_TIM_ACTIVE_CHANNEL_2, HAL_TIM_ACTIVE_CHANNEL_3, HAL_TIM_ACTIVE_CHANNEL_4
};

typedef struct { uint8_t r, g, b; } Color;

static uint32_t tim1_clock_hz(void)
{
    /* APB2 timers run at 2 x PCLK2 unless the APB2 prescaler is 1 */
    RCC_ClkInitTypeDef clk;
    uint32_t latency;
    HAL_RCC_GetClockConfig(&clk, &latency);
    return (clk.APB2CLKDivider == RCC_HCLK_DIV1) ? HAL_RCC_GetPCLK2Freq() : 2U * HAL_RCC_GetPCLK2Freq();
}

static uint16_t ticks(uint32_t ns)
{
    return (uint16_t)(((uint64_t)test_ws_timclk * ns + 500000000ULL) / 1000000000ULL);
}

/* One LED = 24 compare values, G R B, most significant bit first. */
static void put_led(uint16_t *out, Color c)
{
    const uint8_t bytes[3] = { c.g, c.r, c.b };
    uint8_t i, bit;
    for (i = 0; i < 3U; ++i) {
        const uint8_t v = (uint8_t)((bytes[i] * WS_BRIGHTNESS + 127U) / 255U);
        for (bit = 0x80U; bit != 0U; bit >>= 1) {
            *out++ = (v & bit) ? ccr_1 : ccr_0;
        }
    }
}

static Color wheel(uint8_t pos)     /* 0..255 around the colour wheel */
{
    Color c;
    if (pos < 85U)       { c.r = (uint8_t)(255U - pos * 3U); c.g = (uint8_t)(pos * 3U); c.b = 0; }
    else if (pos < 170U) { pos -= 85U;  c.r = 0; c.g = (uint8_t)(255U - pos * 3U); c.b = (uint8_t)(pos * 3U); }
    else                 { pos -= 170U; c.r = (uint8_t)(pos * 3U); c.g = 0; c.b = (uint8_t)(255U - pos * 3U); }
    return c;
}

/* Colour of LED i on channel ch at time t (ms since the pattern started). */
static Color pattern_color(uint8_t pattern, uint8_t ch, uint32_t i, uint32_t t)
{
    static const Color fill[4]    = { {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255} };
    static const Color channel[4] = { {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 180, 0} };
    const Color off = { 0, 0, 0 }, white = { 255, 255, 255 }, dim = { 8, 8, 8 };
    const Color green = { 0, 255, 0 }, blue = { 0, 0, 255 }, red = { 255, 0, 0 };

    switch (pattern) {
    case 1:  return fill[(t / 1000U) % 4U];
    case 2:  return channel[ch];
    case 3:
        if (i == 0U)                  { return green; }
        if (i == WS_LEDS - 1U)        { return red; }
        if (i % WS_MARK_EVERY == 0U)  { return blue; }
        return dim;
    case 4:  return (i == (t / 60U) % WS_LEDS) ? white : off;
    default: return wheel((uint8_t)(i * 256U / WS_LEDS + t / 8U));
    }
}

/* pattern lengths in ms: fill, channel, markers, chase, rainbow */
static const uint32_t pattern_ms[5] = { 4000U, 2000U, 3000U, WS_LEDS * 60U, 4000U };

static void select_pattern(uint32_t now, uint8_t *pattern, uint32_t *t)
{
    uint32_t total = 0, k;
    for (k = 0; k < 5U; ++k) { total += pattern_ms[k]; }
    *t = now % total;
    for (k = 0; k < 4U && *t >= pattern_ms[k]; ++k) { *t -= pattern_ms[k]; }
    *pattern = (uint8_t)(k + 1U);
}

static void send_frame(void)
{
    uint8_t ch;
    for (ch = 0; ch < 4U; ++ch) {
        if ((test_ws_channels & (1U << ch)) == 0U) {
            continue;
        }
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
        if (SCB->CCR & SCB_CCR_DC_Msk) {    /* D-cache on: DMA reads RAM, so write the cache back */
            SCB_CleanDCache_by_Addr((uint32_t *)dma_buf[ch], (int32_t)sizeof dma_buf[ch]);
        }
#endif
        busy |= (uint8_t)(1U << ch);
        if (HAL_TIM_PWM_Start_DMA(&htim1, tim_channel[ch], (uint32_t *)dma_buf[ch], WS_SLOTS) != HAL_OK) {
            busy &= (uint8_t)~(1U << ch);
            ++test_ws_errors;
        }
    }
}

void TestWs_Init(void)
{
    uint8_t ch;
    test_ws_timclk = tim1_clock_hz();
    /* 1.25 us per bit; 0 bit = 0.40 us high, 1 bit = 0.80 us high */
    __HAL_TIM_SET_AUTORELOAD(&htim1, ticks(1250U) - 1U);
    ccr_0 = ticks(400U);
    ccr_1 = ticks(800U);
    test_ws_channels = 0;
    for (ch = 0; ch < 4U; ++ch) {
        if (htim1.hdma[dma_id[ch]] != NULL) {   /* linked by CubeMX = channel set up */
            test_ws_channels |= (uint8_t)(1U << ch);
        }
    }
    memset(dma_buf, 0, sizeof dma_buf);
}

void TestWs_Loop(void)
{
    const uint32_t now = HAL_GetTick();
    uint8_t pattern, ch;
    uint32_t t, i;

    if (busy != 0U || now - last_frame_ms < WS_FRAME_MS) {
        return;
    }
    last_frame_ms = now;
    select_pattern(now, &pattern, &t);
    test_ws_pattern = pattern;

    for (ch = 0; ch < 4U; ++ch) {
        uint16_t *out = dma_buf[ch];
        for (i = 0; i < WS_LEDS; ++i, out += 24) {
            put_led(out, pattern_color(pattern, ch, i, t));
        }
        /* the reset slots stay 0 (line low) from TestWs_Init */
    }
    send_frame();
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    uint8_t ch;
    if (htim->Instance != TIM1) {
        return;
    }
    for (ch = 0; ch < 4U; ++ch) {
        if (htim->Channel == active[ch]) {
            HAL_TIM_PWM_Stop_DMA(&htim1, tim_channel[ch]);
            busy &= (uint8_t)~(1U << ch);
            if (busy == 0U) {
                ++test_ws_frames;
            }
        }
    }
}
