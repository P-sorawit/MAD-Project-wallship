/* TEST 2b: ONE 7x8 table, keys + LEDs together, stand-alone (no other driver file).
 *
 * CubeMX needs only what the LED test needs: TIM1 CH1 PWM + DMA2 (Memory To
 * Peripheral, Half Word). The 8 column + 7 row pins are set up here, so they
 * need no CubeMX setting (leave them unused there).
 *
 * Wire the table under test (same pins for every table, test them one by one):
 *   column A..H -> PD0 CN9-25, PD1 CN9-27, PD14 CN7-16, PD3 CN9-10,
 *                  PD4 CN9-8,  PD5 CN9-6,  PD6 CN9-4,   PD7 CN9-2
 *   row 1..7    -> PF0 CN9-21, PF1 CN9-19, PF2 CN9-17, PF3 CN9-7,
 *                  PF4 CN10-11, PF5 CN9-9, PE15 CN10-30
 *   LED DIN     -> PE9 CN10-4 (TIM1 CH1) through the 74AHCT125
 *   (rows use the chip pull-up; external 10k to 3.3 V is fine too)
 *
 * What you see: all 56 LEDs glow dim. Press a key and the LED of that cell
 * steps to the next colour: red, orange, yellow, green, cyan, blue, purple,
 * white, red ... When every key has been pressed once the table flashes green.
 *   one press = one step  -> key and LED of that cell are fine
 *   skips 2+ colours      -> the key bounces (contact / solder)
 *   another LED changes   -> set LED_SERPENTINE / LED_FLIP_X / LED_FLIP_Y below,
 *                            then copy the same values to WS2812_* in app_config.h
 *
 * Several keys at once: a key that could be a "ghost" (4th corner of a
 * rectangle of held keys, only possible with a missing/reversed/shorted
 * diode) is NOT coloured, it is counted in test_tbl_ghost instead. If three
 * keys held in an L make it count up, the bad diode is on the corner key of
 * the L (the one sharing a row with one key and a column with the other).
 *
 * Live Expressions:
 *   test_tbl_presses     key presses counted
 *   test_tbl_seen        distinct keys pressed (target 56)
 *   test_tbl_last_x/_y   last key: x 0..7 = A..H, y 0..6 = row 1..7
 *   test_tbl_raw[x]      rows seen low on column x right now (bit y = row y+1)
 *   test_tbl_led_ok      0 = TIM1 CH1 has no DMA in CubeMX (keys still work)
 *   test_tbl_ghost       presses dropped as possible ghosts; _x/_y = the last one
 */
#include "main.h"
#include "hwtests.h"
#include <stdbool.h>
#include <string.h>

/* ---------------- settings ---------------- */
#define COLS            8U
#define ROWS            7U
#define LEDS            (COLS * ROWS)
#define LED_SERPENTINE  0     /* 1 = LED string snakes (rows 2, 4, 6 run H -> A) */
#define LED_FLIP_X      0     /* 1 = first LED is on column H */
#define LED_FLIP_Y      0     /* 1 = first LED is on row 7 */
#define LED_BRIGHTNESS  64U   /* 0..255 */
#define SCAN_MS         2U    /* scan all 8 columns every 2 ms */
#define DEBOUNCE_SCANS  4U    /* key must read the same 4 scans in a row (8 ms) */
#define SETTLE_US       20U   /* wait after pulling a column low, before reading rows */
#define FLASH_MS        1500U /* "all keys done": 3 green flashes */

typedef struct { GPIO_TypeDef *port; uint16_t pin; } Pin;

static const Pin col_pins[COLS] = {
    {GPIOD, GPIO_PIN_0}, {GPIOD, GPIO_PIN_1}, {GPIOD, GPIO_PIN_14}, {GPIOD, GPIO_PIN_3},
    {GPIOD, GPIO_PIN_4}, {GPIOD, GPIO_PIN_5}, {GPIOD, GPIO_PIN_6},  {GPIOD, GPIO_PIN_7}
};
static const Pin row_pins[ROWS] = {
    {GPIOF, GPIO_PIN_0}, {GPIOF, GPIO_PIN_1}, {GPIOF, GPIO_PIN_2}, {GPIOF, GPIO_PIN_3},
    {GPIOF, GPIO_PIN_4}, {GPIOF, GPIO_PIN_5}, {GPIOE, GPIO_PIN_15}
};

extern TIM_HandleTypeDef htim1;

volatile uint16_t test_tbl_presses;
volatile uint8_t  test_tbl_seen;
volatile uint8_t  test_tbl_last_x, test_tbl_last_y;
volatile uint8_t  test_tbl_raw[COLS];
volatile uint8_t  test_tbl_led_ok;
volatile uint16_t test_tbl_ghost;
volatile uint8_t  test_tbl_ghost_x, test_tbl_ghost_y;

typedef struct { uint8_t r, g, b; } Color;

static const Color colors[] = {
    {255, 0, 0}, {255, 100, 0}, {255, 220, 0}, {0, 255, 0},
    {0, 220, 255}, {0, 0, 255}, {160, 0, 255}, {255, 255, 255}
};
#define COLOR_COUNT (sizeof colors / sizeof colors[0])
static const Color dim = {10, 10, 16};
static const Color green = {0, 255, 0};

/* keys */
static uint8_t stable[COLS];             /* debounced: bit y = key (x, y) held */
static uint8_t count[COLS][ROWS];        /* scans in a row that differ from stable */
static uint8_t step[COLS][ROWS];         /* 0 = never pressed, 1..COLOR_COUNT = colour */
static uint32_t last_scan_ms;

/* LEDs: 24 compare values per LED (G R B, MSB first) + 300 us low */
#define RESET_SLOTS 240U
static uint16_t led_dma[LEDS * 24U + RESET_SLOTS] __attribute__((aligned(32)));
static uint16_t ccr_0, ccr_1;
static volatile bool led_busy;
static bool dirty = true;
static bool flashing;
static uint32_t flash_start;

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
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    for (i = 0; i < COLS; ++i) {
        HAL_GPIO_WritePin(col_pins[i].port, col_pins[i].pin, GPIO_PIN_SET); /* released = High-Z */
        g.Pin = col_pins[i].pin;
        g.Mode = GPIO_MODE_OUTPUT_OD;
        g.Pull = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(col_pins[i].port, &g);
    }
    for (i = 0; i < ROWS; ++i) {
        g.Pin = row_pins[i].pin;
        g.Mode = GPIO_MODE_INPUT;
        g.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(row_pins[i].port, &g);
    }
}

static void on_press(uint8_t x, uint8_t y, uint32_t now)
{
    ++test_tbl_presses;
    test_tbl_last_x = x;
    test_tbl_last_y = y;
    if (step[x][y] == 0U && ++test_tbl_seen == LEDS) {
        flashing = true;
        flash_start = now;
    }
    step[x][y] = (uint8_t)(step[x][y] % COLOR_COUNT + 1U);
    dirty = true;
}

/* Key (x, y) may be a ghost if another column holds a key in row y AND
 * shares a second held row with column x (three keys in an L + this corner). */
static bool maybe_ghost(uint8_t x, uint8_t y)
{
    const uint8_t bit = (uint8_t)(1U << y);
    uint8_t c;
    for (c = 0; c < COLS; ++c) {
        if (c != x && (test_tbl_raw[c] & bit) != 0U &&
            (test_tbl_raw[c] & test_tbl_raw[x] & (uint8_t)~bit) != 0U) {
            return true;
        }
    }
    return false;
}

/* Pull one column low at a time, read the 7 rows: a pressed key pulls its row low. */
static void scan_keys(uint32_t now)
{
    uint8_t x, y;
    for (x = 0; x < COLS; ++x) {
        uint8_t raw = 0;
        HAL_GPIO_WritePin(col_pins[x].port, col_pins[x].pin, GPIO_PIN_RESET);
        delay_us(SETTLE_US);
        for (y = 0; y < ROWS; ++y) {
            if (HAL_GPIO_ReadPin(row_pins[y].port, row_pins[y].pin) == GPIO_PIN_RESET) {
                raw |= (uint8_t)(1U << y);
            }
        }
        HAL_GPIO_WritePin(col_pins[x].port, col_pins[x].pin, GPIO_PIN_SET);
        test_tbl_raw[x] = raw;

        for (y = 0; y < ROWS; ++y) {
            const uint8_t bit = (uint8_t)(1U << y);
            if ((raw & bit) == (stable[x] & bit)) {
                count[x][y] = 0;
            } else if (++count[x][y] >= DEBOUNCE_SCANS) {
                count[x][y] = 0;
                stable[x] ^= bit;
                if ((raw & bit) == 0U) {
                    continue;                  /* release: nothing to do */
                }
                if (maybe_ghost(x, y)) {
                    ++test_tbl_ghost;
                    test_tbl_ghost_x = x;
                    test_tbl_ghost_y = y;
                } else {
                    on_press(x, y, now);
                }
            }
        }
    }
}

/* ---------------- LEDs ---------------- */

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

static void leds_init(void)
{
    const uint32_t clk = tim1_clock_hz();
    __HAL_TIM_SET_AUTORELOAD(&htim1, ns_to_ticks(clk, 1250U) - 1U);   /* 1.25 us per bit */
    ccr_0 = ns_to_ticks(clk, 400U);
    ccr_1 = ns_to_ticks(clk, 800U);
    test_tbl_led_ok = htim1.hdma[TIM_DMA_ID_CC1] != NULL;
    memset(led_dma, 0, sizeof led_dma);
}

/* Position of cell (x, y) along the LED string. */
static uint8_t led_index(uint8_t x, uint8_t y)
{
    if (LED_FLIP_X) { x = (uint8_t)(COLS - 1U - x); }
    if (LED_FLIP_Y) { y = (uint8_t)(ROWS - 1U - y); }
    if (LED_SERPENTINE && (y & 1U)) { x = (uint8_t)(COLS - 1U - x); }
    return (uint8_t)(y * COLS + x);
}

static void put_led(uint8_t index, Color c)
{
    const uint8_t bytes[3] = { c.g, c.r, c.b };
    uint16_t *out = &led_dma[index * 24U];
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
    const bool green_on = flashing && ((now - flash_start) / 250U) % 2U == 0U;
    uint8_t x, y;
    for (x = 0; x < COLS; ++x) {
        for (y = 0; y < ROWS; ++y) {
            const uint8_t s = step[x][y];
            put_led(led_index(x, y), green_on ? green : (s == 0U ? dim : colors[s - 1U]));
        }
    }
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if (SCB->CCR & SCB_CCR_DC_Msk) {    /* D-cache on: DMA reads RAM, write the cache back */
        SCB_CleanDCache_by_Addr((uint32_t *)led_dma, (int32_t)sizeof led_dma);
    }
#endif
    led_busy = true;
    if (HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *)led_dma, LEDS * 24U + RESET_SLOTS) != HAL_OK) {
        led_busy = false;
        dirty = true;               /* try again next loop */
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

    if (now - last_scan_ms >= SCAN_MS) {
        last_scan_ms = now;
        scan_keys(now);
    }
    if (flashing) {
        flashing = (now - flash_start) < FLASH_MS;
        dirty = true;               /* keep redrawing while flashing, once more after */
    }
    if (dirty && test_tbl_led_ok && !led_busy) {
        dirty = false;
        show_leds(now);
    }
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
        HAL_TIM_PWM_Stop_DMA(&htim1, TIM_CHANNEL_1);
        led_busy = false;
    }
}
