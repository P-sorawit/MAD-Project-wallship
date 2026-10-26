/* STM32 side of the touch panels (PINOUT.md section 4).
 * CubeMX: T_CLK, T_MOSI, T_CS, T_CS2 = GPIO_Output (Very High, initial High
 *         for the CS pins); T_MISO = GPIO_Input; T_IRQ, T_IRQ2 = GPIO_EXTI
 *         falling edge, pull-up, NVIC on.
 *
 * Calibration (TODO on hardware): run in Debug, add a Live Expression for
 * Touch_LastRaw values (or watch `raw[]`), tap the four corners, and put the
 * readings into TOUCH_RAW_* in app_config.h.
 */
#include "main.h"
#include "touch.h"
#include "app_config.h"
#include "gfx.h"

#ifndef TOUCH_RAW_X_MIN
#define TOUCH_RAW_X_MIN  { 300U, 300U }   /* raw at screen x = 0   */
#define TOUCH_RAW_X_MAX  { 3800U, 3800U } /* raw at screen x = 239 */
#define TOUCH_RAW_Y_MIN  { 300U, 300U }
#define TOUCH_RAW_Y_MAX  { 3800U, 3800U }
#define TOUCH_SWAP_XY    { 0U, 0U }       /* 1 if the panel's X runs along the long side */
#endif

#define SAMPLES 8

static const uint16_t x_min[2] = TOUCH_RAW_X_MIN, x_max[2] = TOUCH_RAW_X_MAX;
static const uint16_t y_min[2] = TOUCH_RAW_Y_MIN, y_max[2] = TOUCH_RAW_Y_MAX;
static const uint8_t swap_xy[2] = TOUCH_SWAP_XY;

static GPIO_TypeDef *const cs_port[2] = { T_CS_GPIO_Port, T_CS2_GPIO_Port };
static const uint16_t cs_pin[2] = { T_CS_Pin, T_CS2_Pin };
static GPIO_TypeDef *const irq_port[2] = { T_IRQ_GPIO_Port, T_IRQ2_GPIO_Port };
static const uint16_t irq_pin[2] = { T_IRQ_Pin, T_IRQ2_Pin };

static volatile bool pen_irq[2];
static bool pen_down[2];
static uint16_t raw[2][2];

/* Overrides the weak hook in app.c (EXTI ISR context). */
void App_TouchIrq(uint8_t panel)
{
    if (panel < 2U) {
        pen_irq[panel] = true;
    }
}

static bool touching(uint8_t p)
{
    return HAL_GPIO_ReadPin(irq_port[p], irq_pin[p]) == GPIO_PIN_RESET;
}

/* Half an XPT2046 clock: the chip allows at most 2.5 MHz, a bare GPIO
 * toggle at 216 MHz is about twice that. */
static void half_clock(void)
{
    volatile uint32_t n = SystemCoreClock / 4000000U;   /* >= 2 cycles each: >= 0.5 us */
    while (n-- != 0U) {
    }
}

static uint16_t xfer(uint8_t p, uint8_t cmd)
{
    uint16_t v = 0;
    int i;
    HAL_GPIO_WritePin(cs_port[p], cs_pin[p], GPIO_PIN_RESET);
    for (i = 7; i >= 0; --i) {   /* DIN is sampled on the rising edge */
        HAL_GPIO_WritePin(T_MOSI_GPIO_Port, T_MOSI_Pin,
                          ((cmd >> i) & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        half_clock();
        HAL_GPIO_WritePin(T_CLK_GPIO_Port, T_CLK_Pin, GPIO_PIN_SET);
        half_clock();
        HAL_GPIO_WritePin(T_CLK_GPIO_Port, T_CLK_Pin, GPIO_PIN_RESET);
    }
    HAL_GPIO_WritePin(T_MOSI_GPIO_Port, T_MOSI_Pin, GPIO_PIN_RESET);
    /* 1 null bit + 12 data bits + 3 zeros. DOUT changes on the falling edge,
     * so read it while the clock is high (like SPI mode 0). */
    for (i = 0; i < 16; ++i) {
        half_clock();
        HAL_GPIO_WritePin(T_CLK_GPIO_Port, T_CLK_Pin, GPIO_PIN_SET);
        half_clock();
        v = (uint16_t)((v << 1) |
            (HAL_GPIO_ReadPin(T_MISO_GPIO_Port, T_MISO_Pin) == GPIO_PIN_SET ? 1U : 0U));
        HAL_GPIO_WritePin(T_CLK_GPIO_Port, T_CLK_Pin, GPIO_PIN_RESET);
    }
    HAL_GPIO_WritePin(cs_port[p], cs_pin[p], GPIO_PIN_SET);
    return (uint16_t)((v >> 3) & 0x0FFFU);
}

static int scale(uint16_t r, uint16_t lo, uint16_t hi, int size)
{
    long v;
    if (hi == lo) {
        return 0;
    }
    v = ((long)r - (long)lo) * (size - 1) / ((long)hi - (long)lo); /* hi < lo flips */
    return v < 0 ? 0 : (v >= size ? size - 1 : (int)v);
}

void Touch_Init(void)
{
    uint8_t p;
    for (p = 0; p < 2U; ++p) {
        HAL_GPIO_WritePin(cs_port[p], cs_pin[p], GPIO_PIN_SET);
        pen_irq[p] = false;
        pen_down[p] = false;
    }
    HAL_GPIO_WritePin(T_CLK_GPIO_Port, T_CLK_Pin, GPIO_PIN_RESET);
    /* The XPT2046 only drives T_IRQ after a command with PD1 PD0 = 00. */
    for (p = 0; p < 2U; ++p) {
        (void)xfer(p, 0xD0);
    }
}

bool Touch_Poll(uint8_t p, int *x, int *y)
{
    uint32_t sx = 0, sy = 0;
    uint16_t rx, ry;
    int i, good = 0;
    if (p > 1U) {
        return false;
    }
    if (!touching(p)) {
        pen_down[p] = false;     /* lifted: next tap may fire */
        pen_irq[p] = false;
        return false;
    }
    if (pen_down[p] || !pen_irq[p]) {
        return false;            /* still the same tap */
    }
    pen_irq[p] = false;
    for (i = 0; i < SAMPLES; ++i) {
        const uint16_t a = xfer(p, 0xD0), b = xfer(p, 0x90); /* X, Y 12-bit */
        if (a > 50U && a < 4050U && b > 50U && b < 4050U) {
            sx += a;
            sy += b;
            ++good;
        }
    }
    if (good < SAMPLES / 2 || !touching(p)) {
        return false;            /* noisy or released during the read */
    }
    pen_down[p] = true;
    rx = (uint16_t)(sx / (uint32_t)good);
    ry = (uint16_t)(sy / (uint32_t)good);
    raw[p][0] = rx;
    raw[p][1] = ry;
    if (swap_xy[p]) {
        const uint16_t t = rx; rx = ry; ry = t;
    }
    *x = scale(rx, x_min[p], x_max[p], GFX_W);
    *y = scale(ry, y_min[p], y_max[p], GFX_H);
    return true;
}

void Touch_LastRaw(uint8_t p, uint16_t *rx, uint16_t *ry)
{
    if (p < 2U) {
        *rx = raw[p][0];
        *ry = raw[p][1];
    }
}
