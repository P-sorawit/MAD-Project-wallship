/* TEST 2b: all 4 tables (7x8 each), keys + LEDs together, stand-alone.
 *
 * No CubeMX setting needed: the 36 key pins, the 4 LED pins, TIM1 and the 4
 * DMA2 streams are all set up here. The DMA is checked by polling, no
 * interrupt is used, so nothing clashes with what CubeMX may have set up.
 * Defines no HAL callback, so it can sit in the build next to other tests.
 * Works with fewer tables connected too (missing rows just read "not pressed").
 *
 * Wiring (= PINOUT.md): the 8 columns are shared by all 4 tables
 *   column A..H -> PD0 CN9-25, PD1 CN9-27, PD14 CN7-16, PD3 CN9-10,
 *                  PD4 CN9-8,  PD5 CN9-6,  PD6 CN9-4,   PD7 CN9-2
 *   rows 1..7   table 0 P1 fleet : PF0, PF1, PF2, PF3, PF4, PF5, PE15
 *               table 1 P1 attack: PF7, PF8, PF9, PF10, PB10, PF12, PF13
 *               table 2 P2 fleet : PF14, PF15, PG0, PG1, PG2, PG3, PB11
 *               table 3 P2 attack: PD11, PD12, PG9, PD13, PB2, PG14, PB6
 *   LED DIN     table 0..3 -> PE9 CN10-4, PE11 CN10-6, PE13 CN10-10, PE14 CN10-28
 *               (TIM1 CH1..CH4, each through the 74AHCT125 + 330R)
 *
 * What you see: every table glows dim in its own tint (0 white, 1 blue,
 * 2 amber, 3 purple) so you can tell which LED strip is which. Press a key
 * and the LED of that cell steps to the next colour: red, orange, yellow,
 * green, cyan, blue, purple, white, red ... A table whose 56 keys have all
 * been pressed flashes green 3 times.
 *   one press = one step  -> key and LED of that cell are fine
 *   skips 2+ colours      -> the key bounces (contact / solder)
 *   another LED changes   -> see last_x/last_y: wrong x/y = key wiring
 *                            (KEY_FLIP_*), right x/y = LED order (LED_FLIP_*,
 *                            LED_SERPENTINE). Copy the values to app_config.h.
 * Several keys at once: a possible "ghost" (4th corner of a rectangle of held
 * keys, only with a missing/reversed/shorted diode) is not coloured but
 * counted in test_tbl_ghost. The bad diode is on the corner key of the L.
 *
 * Live Expressions:
 *   test_tbl_presses            key presses counted (all tables)
 *   test_tbl_seen[t]            distinct keys pressed on table t (target 56)
 *   test_tbl_last_table/_x/_y   last key: x 0..7 = A..H, y 0..6 = row 1..7
 *   test_tbl_raw[x]             rows seen low on column x now (bit = row 0..27)
 *   test_tbl_ghost              presses dropped as possible ghosts
 *   test_tbl_frames             LED frames sent (keeps rising while keys change)
 */
#include "main.h"
#include "hwtests.h"
#include <stdbool.h>
#include <string.h>

/* ---------------- settings ---------------- */
#define TABLES          4U
#define COLS            8U
#define ROWS            7U                  /* per table */
#define ALL_ROWS        (TABLES * ROWS)     /* 28 */
#define LEDS            (COLS * ROWS)       /* 56 per table */
/* per table { 0, 1, 2, 3 } */
static const uint8_t KEY_FLIP_X[TABLES]     = { 0, 0, 1, 1 };  /* 1 = column wires A..H reversed */
static const uint8_t KEY_FLIP_Y[TABLES]     = { 0, 0, 0, 0 };  /* 1 = row wires 1..7 reversed */
static const uint8_t LED_FLIP_X[TABLES]     = { 0, 0, 0, 0 };  /* 1 = first LED is on column H */
static const uint8_t LED_FLIP_Y[TABLES]     = { 0, 0, 0, 0 };  /* 1 = first LED is on row 7 */
static const uint8_t LED_SERPENTINE[TABLES] = { 0, 0, 0, 0 };  /* 1 = rows 2, 4, 6 run H -> A */
#define LED_BRIGHTNESS  64U     /* 0..255 (all 224 LEDs white = about 3.4 A) */
#define SCAN_MS         2U      /* scan all 8 columns every 2 ms */
#define DEBOUNCE_SCANS  4U      /* key must read the same 4 scans in a row (8 ms) */
#define SETTLE_US       20U     /* wait after pulling a column low, before reading rows */
#define FLASH_MS        1500U   /* "table done": 3 green flashes */

typedef struct { GPIO_TypeDef *port; uint16_t pin; } Pin;

static const Pin col_pins[COLS] = {
    {GPIOD, GPIO_PIN_0}, {GPIOD, GPIO_PIN_1}, {GPIOD, GPIO_PIN_14}, {GPIOD, GPIO_PIN_3},
    {GPIOD, GPIO_PIN_4}, {GPIOD, GPIO_PIN_5}, {GPIOD, GPIO_PIN_6},  {GPIOD, GPIO_PIN_7}
};
static const Pin row_pins[ALL_ROWS] = {
    {GPIOF, GPIO_PIN_0},  {GPIOF, GPIO_PIN_1},  {GPIOF, GPIO_PIN_2},  {GPIOF, GPIO_PIN_3},   /* table 0 */
    {GPIOF, GPIO_PIN_4},  {GPIOF, GPIO_PIN_5},  {GPIOE, GPIO_PIN_15},
    {GPIOF, GPIO_PIN_7},  {GPIOF, GPIO_PIN_8},  {GPIOF, GPIO_PIN_9},  {GPIOF, GPIO_PIN_10},  /* table 1 */
    {GPIOB, GPIO_PIN_10}, {GPIOF, GPIO_PIN_12}, {GPIOF, GPIO_PIN_13},
    {GPIOF, GPIO_PIN_14}, {GPIOF, GPIO_PIN_15}, {GPIOG, GPIO_PIN_0},  {GPIOG, GPIO_PIN_1},   /* table 2 */
    {GPIOG, GPIO_PIN_2},  {GPIOG, GPIO_PIN_3},  {GPIOB, GPIO_PIN_11},
    {GPIOD, GPIO_PIN_11}, {GPIOD, GPIO_PIN_12}, {GPIOG, GPIO_PIN_9},  {GPIOD, GPIO_PIN_13},  /* table 3 */
    {GPIOB, GPIO_PIN_2},  {GPIOG, GPIO_PIN_14}, {GPIOB, GPIO_PIN_6}
};

volatile uint16_t test_tbl_presses;
volatile uint8_t  test_tbl_seen[TABLES];
volatile uint8_t  test_tbl_last_table, test_tbl_last_x, test_tbl_last_y;
volatile uint32_t test_tbl_raw[COLS];
volatile uint16_t test_tbl_ghost;
volatile uint32_t test_tbl_frames;

typedef struct { uint8_t r, g, b; } Color;

static const Color colors[] = {
    {255, 0, 0}, {255, 100, 0}, {255, 220, 0}, {0, 255, 0},
    {0, 220, 255}, {0, 0, 255}, {160, 0, 255}, {255, 255, 255}
};
#define COLOR_COUNT (sizeof colors / sizeof colors[0])
static const Color tint[TABLES] = { {12, 12, 12}, {0, 8, 24}, {20, 8, 0}, {14, 0, 18} };
static const Color green = {0, 255, 0};

/* keys */
static uint32_t stable[COLS];                /* debounced: bit r = key (column, row r) held */
static uint8_t count[COLS][ALL_ROWS];        /* scans in a row that differ from stable */
static uint8_t step[TABLES][COLS][ROWS];     /* 0 = never pressed, 1..COLOR_COUNT = colour */
static uint32_t last_scan_ms;

/* LEDs: 24 compare values per LED (G R B, MSB first) + 300 us low */
#define RESET_SLOTS 240U
#define SLOTS       (LEDS * 24U + RESET_SLOTS)
static uint16_t led_dma[TABLES][SLOTS] __attribute__((aligned(32)));
static uint16_t ccr_0, ccr_1;
static bool sending;
static bool dirty = true;
static bool flashing[TABLES];
static uint32_t flash_start[TABLES];

/* ---------------- keys ---------------- */

static void delay_us(uint32_t us)
{
    /* each pass takes 2+ CPU cycles, so this waits at least `us` */
    volatile uint32_t n = us * (SystemCoreClock / 1000000U) / 2U;
    while (n-- != 0U) {
    }
}

static void keys_init(void)
{
    GPIO_InitTypeDef g = {0};
    uint8_t i;
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    for (i = 0; i < COLS; ++i) {
        HAL_GPIO_WritePin(col_pins[i].port, col_pins[i].pin, GPIO_PIN_SET); /* released = High-Z */
        g.Pin = col_pins[i].pin;
        g.Mode = GPIO_MODE_OUTPUT_OD;
        g.Pull = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(col_pins[i].port, &g);
    }
    for (i = 0; i < ALL_ROWS; ++i) {
        g.Pin = row_pins[i].pin;
        g.Mode = GPIO_MODE_INPUT;
        g.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(row_pins[i].port, &g);
    }
}

static void on_press(uint8_t t, uint8_t x, uint8_t y, uint32_t now)
{
    ++test_tbl_presses;
    test_tbl_last_table = t;
    test_tbl_last_x = x;
    test_tbl_last_y = y;
    if (step[t][x][y] == 0U && ++test_tbl_seen[t] == LEDS) {
        flashing[t] = true;
        flash_start[t] = now;
    }
    step[t][x][y] = (uint8_t)(step[t][x][y] % COLOR_COUNT + 1U);
    dirty = true;
}

/* Key (col, row) may be a ghost if another column holds a key in this row
 * AND shares a second held row with this column (an L of three + this corner). */
static bool maybe_ghost(uint8_t col, uint8_t row)
{
    const uint32_t bit = UINT32_C(1) << row;
    uint8_t c;
    for (c = 0; c < COLS; ++c) {
        if (c != col && (test_tbl_raw[c] & bit) != 0U &&
            (test_tbl_raw[c] & test_tbl_raw[col] & ~bit) != 0U) {
            return true;
        }
    }
    return false;
}

static void key_pressed(uint8_t col, uint8_t row, uint32_t now)
{
    const uint8_t t = (uint8_t)(row / ROWS);
    uint8_t x = col, y = (uint8_t)(row % ROWS);
    if (maybe_ghost(col, row)) {
        ++test_tbl_ghost;
        return;
    }
    if (KEY_FLIP_X[t]) { x = (uint8_t)(COLS - 1U - x); }
    if (KEY_FLIP_Y[t]) { y = (uint8_t)(ROWS - 1U - y); }
    on_press(t, x, y, now);
}

/* Pull one column low at a time and read all 28 rows: a pressed key pulls its row low. */
static void scan_keys(uint32_t now)
{
    uint8_t c, r;
    for (c = 0; c < COLS; ++c) {
        uint32_t raw = 0;
        HAL_GPIO_WritePin(col_pins[c].port, col_pins[c].pin, GPIO_PIN_RESET);
        delay_us(SETTLE_US);
        for (r = 0; r < ALL_ROWS; ++r) {
            if (HAL_GPIO_ReadPin(row_pins[r].port, row_pins[r].pin) == GPIO_PIN_RESET) {
                raw |= UINT32_C(1) << r;
            }
        }
        HAL_GPIO_WritePin(col_pins[c].port, col_pins[c].pin, GPIO_PIN_SET);
        test_tbl_raw[c] = raw;

        for (r = 0; r < ALL_ROWS; ++r) {
            const uint32_t bit = UINT32_C(1) << r;
            if ((raw & bit) == (stable[c] & bit)) {
                count[c][r] = 0;
            } else if (++count[c][r] >= DEBOUNCE_SCANS) {
                count[c][r] = 0;
                stable[c] ^= bit;
                if (raw & bit) {
                    key_pressed(c, r, now);    /* press edge only, release is ignored */
                }
            }
        }
    }
}

/* ---------------- LEDs: TIM1 CH1..CH4 PWM, one DMA2 stream each ----------------
 * Every 1.25 us bit, the CCx compare event asks the DMA for the next compare
 * value (CCR preload: it takes effect from the next bit). DMA2 request map,
 * channel 6: TIM1_CH1 Stream1, TIM1_CH2 Stream2, TIM1_CH3 Stream6, TIM1_CH4 Stream4. */

static DMA_Stream_TypeDef *const led_stream[TABLES] = {
    DMA2_Stream1, DMA2_Stream2, DMA2_Stream6, DMA2_Stream4
};
static const uint8_t led_stream_no[TABLES] = { 1, 2, 6, 4 };
static volatile uint32_t *const led_ccr[TABLES] = { &TIM1->CCR1, &TIM1->CCR2, &TIM1->CCR3, &TIM1->CCR4 };
static const uint32_t led_dma_req[TABLES] = { TIM_DIER_CC1DE, TIM_DIER_CC2DE, TIM_DIER_CC3DE, TIM_DIER_CC4DE };

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

static void clear_stream_flags(uint8_t stream)
{
    /* FEIF, DMEIF, TEIF, HTIF, TCIF of one stream; streams 0-3 in LIFCR, 4-7 in HIFCR */
    static const uint8_t shift[4] = { 0, 6, 16, 22 };
    const uint32_t mask = UINT32_C(0x3D) << shift[stream & 3U];
    if (stream < 4U) { DMA2->LIFCR = mask; } else { DMA2->HIFCR = mask; }
}

static void leds_init(void)
{
    const uint32_t clk = tim1_clock_hz();
    GPIO_InitTypeDef g = {0};
    uint8_t t;

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
    for (t = 0; t < TABLES; ++t) {
        *led_ccr[t] = 0;                /* compare 0 = line stays low */
    }
    TIM1->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;
    TIM1->BDTR |= TIM_BDTR_MOE;         /* TIM1 outputs need the main output enable */
    TIM1->EGR = TIM_EGR_UG;             /* load ARR/CCR now */
    TIM1->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;

    for (t = 0; t < TABLES; ++t) {      /* our streams, no interrupt: polled in leds_busy() */
        led_stream[t]->CR = 0;
        while (led_stream[t]->CR & DMA_SxCR_EN) {
        }
    }
    memset(led_dma, 0, sizeof led_dma);
}

static bool leds_busy(void)
{
    uint8_t t;
    bool busy = false;
    for (t = 0; t < TABLES; ++t) {
        if (led_stream[t]->CR & DMA_SxCR_EN) {
            busy = true;                /* the stream clears EN itself when done */
        } else {
            TIM1->DIER &= ~led_dma_req[t];
        }
    }
    if (sending && !busy) {
        sending = false;
        ++test_tbl_frames;
    }
    return busy;
}

static void start_dma(uint8_t t)
{
    DMA_Stream_TypeDef *s = led_stream[t];
    TIM1->DIER &= ~led_dma_req[t];
    s->CR = 0;
    while (s->CR & DMA_SxCR_EN) {
    }
    clear_stream_flags(led_stream_no[t]);
    s->PAR = (uint32_t)led_ccr[t];
    s->M0AR = (uint32_t)led_dma[t];
    s->NDTR = SLOTS;
    s->CR = (6U << DMA_SxCR_CHSEL_Pos)      /* request channel 6 = TIM1 */
          | DMA_SxCR_PL_1                   /* high priority */
          | DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0   /* half-word to half-word */
          | DMA_SxCR_MINC                   /* next value each request */
          | DMA_SxCR_DIR_0;                 /* memory -> peripheral */
    s->CR |= DMA_SxCR_EN;
    TIM1->DIER |= led_dma_req[t];
}

/* Position of cell (x, y) along table t's LED string. */
static uint8_t led_index(uint8_t t, uint8_t x, uint8_t y)
{
    if (LED_FLIP_X[t]) { x = (uint8_t)(COLS - 1U - x); }
    if (LED_FLIP_Y[t]) { y = (uint8_t)(ROWS - 1U - y); }
    if (LED_SERPENTINE[t] && (y & 1U)) { x = (uint8_t)(COLS - 1U - x); }
    return (uint8_t)(y * COLS + x);
}

static void put_led(uint8_t t, uint8_t index, Color c)
{
    const uint8_t bytes[3] = { c.g, c.r, c.b };
    uint16_t *out = &led_dma[t][index * 24U];
    uint8_t i, bit;
    for (i = 0; i < 3U; ++i) {
        const uint8_t v = (uint8_t)((bytes[i] * LED_BRIGHTNESS + 127U) / 255U);
        for (bit = 0x80U; bit != 0U; bit >>= 1) {
            *out++ = (v & bit) ? ccr_1 : ccr_0;
        }
    }
}

static void show_leds(uint32_t now)
{
    uint8_t t, x, y;
    for (t = 0; t < TABLES; ++t) {
        const bool green_on = flashing[t] && ((now - flash_start[t]) / 250U) % 2U == 0U;
        for (x = 0; x < COLS; ++x) {
            for (y = 0; y < ROWS; ++y) {
                const uint8_t s = step[t][x][y];
                put_led(t, led_index(t, x, y), green_on ? green : (s == 0U ? tint[t] : colors[s - 1U]));
            }
        }
    }
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if (SCB->CCR & SCB_CCR_DC_Msk) {    /* D-cache on: DMA reads RAM, write the cache back */
        SCB_CleanDCache_by_Addr((uint32_t *)led_dma, (int32_t)sizeof led_dma);
    }
#endif
    sending = true;
    for (t = 0; t < TABLES; ++t) {
        start_dma(t);
    }
}

/* ---------------- test ---------------- */

void TestTable_Init(void)
{
    keys_init();
    leds_init();
}

void TestTable_Loop(void)
{
    const uint32_t now = HAL_GetTick();
    const bool busy = leds_busy();
    uint8_t t;

    if (now - last_scan_ms >= SCAN_MS) {
        last_scan_ms = now;
        scan_keys(now);
    }
    for (t = 0; t < TABLES; ++t) {
        if (flashing[t]) {
            flashing[t] = (now - flash_start[t]) < FLASH_MS;
            dirty = true;               /* keep redrawing while flashing, once more after */
        }
    }
    if (dirty && !busy) {
        dirty = false;
        show_leds(now);
    }
}
