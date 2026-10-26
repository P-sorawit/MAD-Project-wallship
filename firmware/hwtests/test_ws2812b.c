/* TEST 1b: WS2812B LEDs only, all 4 strips, stand-alone (no other file).
 *
 * No CubeMX setting needed: TIM1, the 4 LED pins and 4 DMA2 streams are set
 * up here, and the DMA is checked by polling (no interrupt, no HAL callback),
 * so it can sit in the build next to the other tests.
 *
 * Wiring: DIN of strip 0..3 <- 330R <- 74AHCT125 1Y..4Y <- 1A..4A <-
 *         PE9 CN10-4, PE11 CN10-6, PE13 CN10-10, PE14 CN10-28 (TIM1 CH1..CH4)
 *         5 V and GND of every strip from the PSU, PSU GND also to CN8-11.
 *
 * Patterns (WS_PATTERN 0 cycles through all, about 17 s per round; set 1..5
 * to stay on one):
 *   1 FILL     all red -> green -> blue -> white, 1 s each
 *              colour order + power: white must stay white to the last LED
 *   2 CHANNEL  strip 0 red, 1 green, 2 blue, 3 yellow    (which strip is which)
 *   3 MARKERS  LED 0 green, every 8th blue (start of each row), last LED red,
 *              the rest dim                               (count + direction)
 *   4 CHASE    one white LED walks from LED 0 to the end
 *              stops at LED n = broken link after n; all light together =
 *              the LEDs' DIN are wired in parallel instead of DOUT -> DIN
 *   5 RAINBOW  moving rainbow
 * The frame is re-sent 50 times a second even when it does not change, so
 * any flicker or random colour while a pattern stands still = bad signal,
 * GND or 5 V on that strip (not the code).
 *
 * Live Expressions: test_ws_pattern (1..5), test_ws_frames (keeps rising),
 *                   test_ws_timclk (216000000)
 */
#include "main.h"
#include "hwtests.h"
#include <stdbool.h>
#include <string.h>

/* ---------------- settings ---------------- */
#define WS_PATTERN      0       /* 0 = cycle all, 1..5 = stay on that pattern */
#define WS_STRIPS       4U
#define WS_LEDS         56U     /* LEDs per strip (8 x 7 table) */
#define WS_MARK_EVERY   8U      /* MARKERS: blue every n LEDs (8 = start of each row) */
#define WS_BRIGHTNESS   64U     /* 0..255. 64 = 25 % (all 224 white = about 3.4 A) */
#define WS_FRAME_MS     20U     /* re-send every 20 ms */

volatile uint8_t  test_ws_pattern;
volatile uint32_t test_ws_frames;
volatile uint32_t test_ws_timclk;

typedef struct { uint8_t r, g, b; } Color;

/* 24 compare values per LED (G R B, MSB first) + 300 us low */
#define RESET_SLOTS 240U
#define SLOTS       (WS_LEDS * 24U + RESET_SLOTS)
static uint16_t dma_buf[WS_STRIPS][SLOTS] __attribute__((aligned(32)));
static uint16_t ccr_0, ccr_1;
static bool sending;
static uint32_t last_frame_ms;

/* ---------------- TIM1 CH1..CH4 PWM, one DMA2 stream each ----------------
 * Every 1.25 us bit, the CCx compare event asks the DMA for the next compare
 * value (CCR preload: it takes effect from the next bit). DMA2 request map,
 * channel 6: TIM1_CH1 Stream1, TIM1_CH2 Stream2, TIM1_CH3 Stream6, TIM1_CH4 Stream4. */

static DMA_Stream_TypeDef *const stream[WS_STRIPS] = {
    DMA2_Stream1, DMA2_Stream2, DMA2_Stream6, DMA2_Stream4
};
static const uint8_t stream_no[WS_STRIPS] = { 1, 2, 6, 4 };
static volatile uint32_t *const ccr[WS_STRIPS] = { &TIM1->CCR1, &TIM1->CCR2, &TIM1->CCR3, &TIM1->CCR4 };
static const uint32_t dma_req[WS_STRIPS] = { TIM_DIER_CC1DE, TIM_DIER_CC2DE, TIM_DIER_CC3DE, TIM_DIER_CC4DE };

static uint32_t tim1_clock_hz(void)
{
    RCC_ClkInitTypeDef clk;
    uint32_t latency;
    HAL_RCC_GetClockConfig(&clk, &latency);
    return (clk.APB2CLKDivider == RCC_HCLK_DIV1) ? HAL_RCC_GetPCLK2Freq() : 2U * HAL_RCC_GetPCLK2Freq();
}

static uint16_t ns_to_ticks(uint32_t clk_hz, uint32_t ns)
{
    return (uint16_t)(((uint64_t)clk_hz * ns + 500000000ULL) / 1000000000ULL);
}

static void clear_stream_flags(uint8_t no)
{
    /* FEIF, DMEIF, TEIF, HTIF, TCIF of one stream; streams 0-3 in LIFCR, 4-7 in HIFCR */
    static const uint8_t shift[4] = { 0, 6, 16, 22 };
    const uint32_t mask = UINT32_C(0x3D) << shift[no & 3U];
    if (no < 4U) { DMA2->LIFCR = mask; } else { DMA2->HIFCR = mask; }
}

static void leds_init(void)
{
    const uint32_t clk = tim1_clock_hz();
    GPIO_InitTypeDef g = {0};
    uint8_t s;

    test_ws_timclk = clk;
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    g.Pin = GPIO_PIN_9 | GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOE, &g);

    ccr_0 = ns_to_ticks(clk, 400U);     /* 0 bit: 0.40 us high */
    ccr_1 = ns_to_ticks(clk, 800U);     /* 1 bit: 0.80 us high */
    TIM1->CR1 = 0;
    TIM1->DIER = 0;
    TIM1->PSC = 0;
    TIM1->ARR = ns_to_ticks(clk, 1250U) - 1U;   /* 1.25 us per bit */
    TIM1->CCMR1 = TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE     /* PWM mode 1 + preload */
                | TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2PE;
    TIM1->CCMR2 = TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3PE
                | TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4PE;
    for (s = 0; s < WS_STRIPS; ++s) {
        *ccr[s] = 0;                    /* compare 0 = line stays low */
    }
    TIM1->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;
    TIM1->BDTR |= TIM_BDTR_MOE;         /* TIM1 outputs need the main output enable */
    TIM1->EGR = TIM_EGR_UG;             /* load ARR/CCR now */
    TIM1->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;

    for (s = 0; s < WS_STRIPS; ++s) {   /* our streams, no interrupt: polled in leds_busy() */
        stream[s]->CR = 0;
        while (stream[s]->CR & DMA_SxCR_EN) {
        }
    }
    memset(dma_buf, 0, sizeof dma_buf);
}

static bool leds_busy(void)
{
    uint8_t s;
    bool busy = false;
    for (s = 0; s < WS_STRIPS; ++s) {
        if (stream[s]->CR & DMA_SxCR_EN) {
            busy = true;                /* the stream clears EN itself when done */
        } else {
            TIM1->DIER &= ~dma_req[s];
        }
    }
    if (sending && !busy) {
        sending = false;
        ++test_ws_frames;
    }
    return busy;
}

static void start_dma(uint8_t s)
{
    DMA_Stream_TypeDef *st = stream[s];
    TIM1->DIER &= ~dma_req[s];
    st->CR = 0;
    while (st->CR & DMA_SxCR_EN) {
    }
    clear_stream_flags(stream_no[s]);
    st->PAR = (uint32_t)ccr[s];
    st->M0AR = (uint32_t)dma_buf[s];
    st->NDTR = SLOTS;
    st->CR = (6U << DMA_SxCR_CHSEL_Pos)     /* request channel 6 = TIM1 */
           | DMA_SxCR_PL_1                  /* high priority */
           | DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0   /* half-word to half-word */
           | DMA_SxCR_MINC                  /* next value each request */
           | DMA_SxCR_DIR_0;                /* memory -> peripheral */
    st->CR |= DMA_SxCR_EN;
    TIM1->DIER |= dma_req[s];
}

static void put_led(uint8_t s, uint32_t index, Color c)
{
    const uint8_t bytes[3] = { c.g, c.r, c.b };
    uint16_t *out = &dma_buf[s][index * 24U];
    uint8_t i, bit;
    for (i = 0; i < 3U; ++i) {
        const uint8_t v = (uint8_t)((bytes[i] * WS_BRIGHTNESS + 127U) / 255U);
        for (bit = 0x80U; bit != 0U; bit >>= 1) {
            *out++ = (v & bit) ? ccr_1 : ccr_0;
        }
    }
}

/* ---------------- patterns ---------------- */

static Color wheel(uint8_t pos)     /* 0..255 around the colour wheel */
{
    Color c;
    if (pos < 85U)       { c.r = (uint8_t)(255U - pos * 3U); c.g = (uint8_t)(pos * 3U); c.b = 0; }
    else if (pos < 170U) { pos -= 85U;  c.r = 0; c.g = (uint8_t)(255U - pos * 3U); c.b = (uint8_t)(pos * 3U); }
    else                 { pos -= 170U; c.r = (uint8_t)(pos * 3U); c.g = 0; c.b = (uint8_t)(255U - pos * 3U); }
    return c;
}

/* Colour of LED i on strip s at time t (ms since the pattern started). */
static Color pattern_color(uint8_t pattern, uint8_t s, uint32_t i, uint32_t t)
{
    static const Color fill[4]    = { {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255} };
    static const Color strip[4]   = { {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 180, 0} };
    const Color off = { 0, 0, 0 }, white = { 255, 255, 255 }, dim = { 8, 8, 8 };
    const Color green = { 0, 255, 0 }, blue = { 0, 0, 255 }, red = { 255, 0, 0 };

    switch (pattern) {
    case 1:  return fill[(t / 1000U) % 4U];
    case 2:  return strip[s];
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
#if WS_PATTERN != 0
    *pattern = (uint8_t)WS_PATTERN;             /* stay on one pattern */
    *t = now % pattern_ms[WS_PATTERN - 1U];
#else
    uint32_t total = 0, k;
    for (k = 0; k < 5U; ++k) { total += pattern_ms[k]; }
    *t = now % total;
    for (k = 0; k < 4U && *t >= pattern_ms[k]; ++k) { *t -= pattern_ms[k]; }
    *pattern = (uint8_t)(k + 1U);
#endif
}

/* ---------------- test ---------------- */

void TestWs_Init(void)
{
    leds_init();
}

void TestWs_Loop(void)
{
    const uint32_t now = HAL_GetTick();
    uint8_t pattern, s;
    uint32_t t, i;

    if (leds_busy() || now - last_frame_ms < WS_FRAME_MS) {
        return;
    }
    last_frame_ms = now;
    select_pattern(now, &pattern, &t);
    test_ws_pattern = pattern;

    for (s = 0; s < WS_STRIPS; ++s) {
        for (i = 0; i < WS_LEDS; ++i) {
            put_led(s, i, pattern_color(pattern, s, i, t));
        }
        /* the reset slots stay 0 (line low) from leds_init */
    }
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if (SCB->CCR & SCB_CCR_DC_Msk) {    /* D-cache on: DMA reads RAM, write the cache back */
        SCB_CleanDCache_by_Addr((uint32_t *)dma_buf, (int32_t)sizeof dma_buf);
    }
#endif
    sending = true;
    for (s = 0; s < WS_STRIPS; ++s) {
        start_dma(s);
    }
}
