/* TEST 7b: both TFT screens (ILI9341) + touch (XPT2046), stand-alone.
 *
 * No CubeMX setting needed: SPI1 and every pin are set up here (SPI by
 * polling, no DMA, no interrupt, no HAL callback), so it can sit in the build
 * next to the other tests. The screens are started on the first
 * TestTftTouch_Loop() call, so selecting another test costs no start-up time.
 *
 * Wiring (= PINOUT.md):            screen P1        screen P2
 *   SCK  PA5 CN7-10, SDI(MOSI) PB5 CN7-13 : both screens (SDO/MISO not used)
 *   CS / DC / RESET                PC8 PC9 PC10     PC11 PD15 PD2
 *   T_CLK PE3, T_DIN PE5, T_DO PE4         : both screens
 *   T_CS                           PE6              PE7
 *   T_IRQ                          PE2              PE8
 *   VCC + LED -> 3.3 V (CN8-7), GND -> GND
 *
 * What each screen shows
 *   border        P1 red, P2 blue (tells which screen is which)
 *   top left      big digit 1 or 2 = top-left corner of the picture
 *   top right     red, green, blue, white squares (colour order)
 *   middle        raw touch value of the last touch: X (cyan), Y (magenta)
 *   lower part    drawing area: touch and drag, yellow dots follow the finger
 *                 (they must land under the finger, else fix TOUCH_* below)
 * Touch the upper part (above the drawing area) to clear the drawing.
 *
 * Calibration: slide a finger along all four edges of a screen, then copy
 * test_tft_raw_min / test_tft_raw_max of that screen into TOUCH_* below and
 * into TOUCH_RAW_* of app_config.h ([0] = X, [1] = Y). If the dots move up
 * when the finger moves right, set TOUCH_SWAP_XY.
 *
 * Live Expressions:
 *   test_tft_irq[s]            1 while screen s is touched (T_IRQ low)
 *   test_tft_touches[s]        number of touches (finger down) on screen s
 *   test_tft_raw[s][0..1]      last raw X, Y (0..4095)
 *   test_tft_raw_min/max[s][]  smallest / largest raw X, Y seen = calibration
 *   test_tft_xy[s][0..1]       last position on the screen (pixels)
 *   test_tft_z1[s]             pressure, read every 100 ms WITHOUT waiting for
 *                              T_IRQ: about 0 when not pressed, rises when pressed.
 *                              Rises but test_tft_irq stays 0 = only the T_IRQ wire.
 *                              Never rises = T_CLK / T_DIN / T_DO / T_CS wiring.
 */
#include "main.h"
#include "hwtests.h"
#include <stdbool.h>

/* ---------------- settings ---------------- */
#define TFT_MADCTL     0x48U   /* as in the game; red/blue swapped -> 0x40, mirrored -> 0x88 */
#define TFT_SPI_BR     1U      /* SPI clock = 108 MHz / 2^(BR+1): 1 = 27 MHz (game), 2 = 13.5, 3 = 6.75 */
/* raw touch value at screen edge x = 0 / x = 239 and y = 0 / y = 319, per screen { P1, P2 } */
static const uint16_t TOUCH_X_MIN[2] = { 300, 300 },   TOUCH_X_MAX[2] = { 3800, 3800 };
static const uint16_t TOUCH_Y_MIN[2] = { 300, 300 },   TOUCH_Y_MAX[2] = { 3800, 3800 };
static const uint8_t  TOUCH_SWAP_XY[2] = { 0, 0 };

#define W 240
#define H 320
#define CANVAS_Y 112           /* drawing area: y 112..316 */
#define BORDER 3

/* RGB565 */
#define BLACK   0x0000U
#define WHITE   0xFFFFU
#define RED     0xF800U
#define GREEN   0x07E0U
#define BLUE    0x001FU
#define YELLOW  0xFFE0U
#define CYAN    0x07FFU
#define MAGENTA 0xF81FU
#define GREY    0x4208U

typedef struct { GPIO_TypeDef *port; uint16_t pin; } Pin;

static const Pin tft_cs[2]  = { {GPIOC, GPIO_PIN_8},  {GPIOC, GPIO_PIN_11} };
static const Pin tft_dc[2]  = { {GPIOC, GPIO_PIN_9},  {GPIOD, GPIO_PIN_15} };
static const Pin tft_rst[2] = { {GPIOC, GPIO_PIN_10}, {GPIOD, GPIO_PIN_2} };
static const Pin t_cs[2]    = { {GPIOE, GPIO_PIN_6},  {GPIOE, GPIO_PIN_7} };
static const Pin t_irq[2]   = { {GPIOE, GPIO_PIN_2},  {GPIOE, GPIO_PIN_8} };
static const Pin t_clk = {GPIOE, GPIO_PIN_3}, t_mosi = {GPIOE, GPIO_PIN_5}, t_miso = {GPIOE, GPIO_PIN_4};

volatile uint8_t  test_tft_irq[2];
volatile uint16_t test_tft_touches[2];
volatile uint16_t test_tft_raw[2][2];
volatile uint16_t test_tft_raw_min[2][2];
volatile uint16_t test_tft_raw_max[2][2];
volatile uint16_t test_tft_xy[2][2];
volatile uint16_t test_tft_z1[2];
static uint32_t z_ms;

static bool screens_ready;
static bool finger[2];              /* finger was down at the last read */
static uint32_t last_ms, digits_ms[2];
static uint32_t touch_pause;        /* busy-wait passes for half an XPT2046 clock */

static void put(Pin p, bool high) { HAL_GPIO_WritePin(p.port, p.pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET); }

/* ---------------- SPI1, transmit only, polled ---------------- */

static void spi_byte(uint8_t b)
{
    while ((SPI1->SR & SPI_SR_TXE) == 0U) {
    }
    *(volatile uint8_t *)&SPI1->DR = b;     /* 8-bit write: one byte, not two */
}

static void spi_flush(void)                 /* wait until the last bit has left */
{
    while ((SPI1->SR & SPI_SR_FTLVL) != 0U) {
    }
    while ((SPI1->SR & SPI_SR_BSY) != 0U) {
    }
}

static void command(uint8_t d, uint8_t cmd, const uint8_t *data, uint8_t len)
{
    put(tft_dc[d], false);
    spi_byte(cmd);
    spi_flush();
    put(tft_dc[d], true);
    while (len--) {
        spi_byte(*data++);
    }
    spi_flush();
}

/* ---------------- ILI9341 ---------------- */

/* Same power-up sequence as 4_tft/tft.c (the lab driver that works on these panels). */
typedef struct { uint8_t cmd, len, data[15]; } InitCmd;
static const InitCmd init_seq[] = {
    {0xCB, 5,  {0x39, 0x2C, 0x00, 0x34, 0x02}},
    {0xCF, 3,  {0x00, 0xC1, 0x30}},
    {0xE8, 3,  {0x85, 0x00, 0x78}},
    {0xEA, 2,  {0x00, 0x00}},
    {0xED, 4,  {0x64, 0x03, 0x12, 0x81}},
    {0xF7, 1,  {0x20}},
    {0xC0, 1,  {0x23}},
    {0xC1, 1,  {0x10}},
    {0xC5, 2,  {0x3E, 0x28}},
    {0xC7, 1,  {0x86}},
    {0x36, 1,  {TFT_MADCTL}},
    {0x3A, 1,  {0x55}},                       /* 16-bit RGB565 */
    {0xB1, 2,  {0x00, 0x18}},
    {0xB6, 3,  {0x08, 0x82, 0x27}},
    {0xF2, 1,  {0x00}},
    {0x26, 1,  {0x01}},
    {0xE0, 15, {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00}},
    {0xE1, 15, {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F}},
};

static void tft_init(uint8_t d)
{
    uint8_t i;
    put(tft_cs[d], true);
    put(tft_rst[d], false);
    HAL_Delay(200);
    put(tft_rst[d], true);
    HAL_Delay(200);
    put(tft_cs[d], false);
    command(d, 0x01, 0, 0);                   /* software reset */
    HAL_Delay(150);
    for (i = 0; i < sizeof init_seq / sizeof init_seq[0]; ++i) {
        command(d, init_seq[i].cmd, init_seq[i].data, init_seq[i].len);
    }
    command(d, 0x11, 0, 0);                   /* sleep out */
    HAL_Delay(120);
    command(d, 0x29, 0, 0);                   /* display on */
    put(tft_cs[d], true);
}

static void fill_rect(uint8_t d, int x, int y, int w, int h, uint16_t color)
{
    uint8_t cols[4], rows[4];
    uint32_t n;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > W) { w = W - x; }
    if (y + h > H) { h = H - y; }
    if (w <= 0 || h <= 0) {
        return;
    }
    cols[0] = (uint8_t)(x >> 8); cols[1] = (uint8_t)x;
    cols[2] = (uint8_t)((x + w - 1) >> 8); cols[3] = (uint8_t)(x + w - 1);
    rows[0] = (uint8_t)(y >> 8); rows[1] = (uint8_t)y;
    rows[2] = (uint8_t)((y + h - 1) >> 8); rows[3] = (uint8_t)(y + h - 1);
    put(tft_cs[d], false);
    command(d, 0x2A, cols, 4);                /* column range */
    command(d, 0x2B, rows, 4);                /* row range */
    command(d, 0x2C, 0, 0);                   /* write pixels; DC stays high */
    for (n = (uint32_t)w * (uint32_t)h; n != 0U; --n) {
        spi_byte((uint8_t)(color >> 8));
        spi_byte((uint8_t)color);
    }
    spi_flush();
    put(tft_cs[d], true);
}

/* 7-segment digit, segments a..g = bits 0..6 */
static void draw_digit(uint8_t d, int x, int y, int w, int h, int t, uint8_t n, uint16_t color)
{
    static const uint8_t seg[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
    const uint8_t s = seg[n % 10U];
    const int half = h / 2;
    fill_rect(d, x, y, w, h, BLACK);
    if (s & 0x01U) { fill_rect(d, x, y, w, t, color); }                     /* a top */
    if (s & 0x02U) { fill_rect(d, x + w - t, y, t, half + t / 2, color); }   /* b upper right */
    if (s & 0x04U) { fill_rect(d, x + w - t, y + half, t, h - half, color); }/* c lower right */
    if (s & 0x08U) { fill_rect(d, x, y + h - t, w, t, color); }             /* d bottom */
    if (s & 0x10U) { fill_rect(d, x, y + half, t, h - half, color); }        /* e lower left */
    if (s & 0x20U) { fill_rect(d, x, y, t, half + t / 2, color); }          /* f upper left */
    if (s & 0x40U) { fill_rect(d, x, y + half - t / 2, w, t, color); }      /* g middle */
}

static void draw_number4(uint8_t d, int x, int y, uint16_t v, uint16_t color)
{
    int i;
    for (i = 3; i >= 0; --i) {
        draw_digit(d, x + i * 24, y, 18, 30, 4, (uint8_t)(v % 10U), color);
        v /= 10U;
    }
}

static void clear_canvas(uint8_t d)
{
    fill_rect(d, BORDER, CANVAS_Y, W - 2 * BORDER, H - BORDER - CANVAS_Y, BLACK);
}

static void draw_layout(uint8_t d)
{
    const uint16_t frame = d == 0U ? RED : BLUE;
    fill_rect(d, 0, 0, W, H, BLACK);
    fill_rect(d, 0, 0, W, BORDER, frame);
    fill_rect(d, 0, H - BORDER, W, BORDER, frame);
    fill_rect(d, 0, 0, BORDER, H, frame);
    fill_rect(d, W - BORDER, 0, BORDER, H, frame);
    draw_digit(d, 12, 10, 26, 44, 6, (uint8_t)(d + 1U), WHITE);   /* 1 / 2 at the top-left corner */
    fill_rect(d, 92, 14, 32, 32, RED);
    fill_rect(d, 128, 14, 32, 32, GREEN);
    fill_rect(d, 164, 14, 32, 32, BLUE);
    fill_rect(d, 200, 14, 32, 32, WHITE);
    draw_number4(d, 12, 68, 0, CYAN);                             /* raw X */
    draw_number4(d, 132, 68, 0, MAGENTA);                         /* raw Y */
    fill_rect(d, BORDER, CANVAS_Y - 4, W - 2 * BORDER, 2, GREY);  /* top of the drawing area */
}

/* ---------------- XPT2046 (bit-banged, <= 2.5 MHz clock) ---------------- */

static void pause(void)
{
    volatile uint32_t n = touch_pause;
    while (n-- != 0U) {
    }
}

static uint16_t touch_read(uint8_t s, uint8_t cmd)
{
    uint16_t v = 0;
    int i;
    put(t_cs[s], false);
    pause();
    for (i = 7; i >= 0; --i) {
        put(t_mosi, ((cmd >> i) & 1U) != 0U);
        pause();
        put(t_clk, true);
        pause();
        put(t_clk, false);
    }
    put(t_mosi, false);
    /* 1 null bit + 12 data bits + 3 zeros. DOUT changes on the falling edge,
     * so read it while the clock is high (like SPI mode 0). */
    for (i = 0; i < 16; ++i) {
        pause();
        put(t_clk, true);
        pause();
        v = (uint16_t)((v << 1) | (HAL_GPIO_ReadPin(t_miso.port, t_miso.pin) == GPIO_PIN_SET ? 1U : 0U));
        put(t_clk, false);
    }
    put(t_cs[s], true);
    return (uint16_t)((v >> 3) & 0x0FFFU);
}

static bool touched(uint8_t s)
{
    return HAL_GPIO_ReadPin(t_irq[s].port, t_irq[s].pin) == GPIO_PIN_RESET;
}

/* Average of 8 readings; false if noisy or the finger left during the read. */
static bool touch_raw(uint8_t s, uint16_t *rx, uint16_t *ry)
{
    uint32_t sx = 0, sy = 0;
    int i, good = 0;
    for (i = 0; i < 8; ++i) {
        const uint16_t a = touch_read(s, 0xD0), b = touch_read(s, 0x90);   /* X, Y */
        if (a > 50U && a < 4050U && b > 50U && b < 4050U) {
            sx += a;
            sy += b;
            ++good;
        }
    }
    if (good < 4 || !touched(s)) {
        return false;
    }
    *rx = (uint16_t)(sx / (uint32_t)good);
    *ry = (uint16_t)(sy / (uint32_t)good);
    return true;
}

static int scale(uint16_t r, uint16_t lo, uint16_t hi, int size)
{
    long v;
    if (hi == lo) {
        return 0;
    }
    v = ((long)r - (long)lo) * (size - 1) / ((long)hi - (long)lo);   /* hi < lo flips */
    return v < 0 ? 0 : (v >= size ? size - 1 : (int)v);
}

static void show_raw(uint8_t s)
{
    draw_number4(s, 12, 68, test_tft_raw[s][0], CYAN);
    draw_number4(s, 132, 68, test_tft_raw[s][1], MAGENTA);
}

static void poll_touch(uint8_t s, uint32_t now)
{
    uint16_t rx, ry;
    int x, y;
    const bool down = touched(s);
    test_tft_irq[s] = down;
    if (!down) {
        if (finger[s]) {
            show_raw(s);                      /* finger lifted: show the last reading */
        }
        finger[s] = false;
        return;
    }
    if (!touch_raw(s, &rx, &ry)) {
        return;
    }
    test_tft_raw[s][0] = rx;
    test_tft_raw[s][1] = ry;
    if (rx < test_tft_raw_min[s][0]) { test_tft_raw_min[s][0] = rx; }
    if (ry < test_tft_raw_min[s][1]) { test_tft_raw_min[s][1] = ry; }
    if (rx > test_tft_raw_max[s][0]) { test_tft_raw_max[s][0] = rx; }
    if (ry > test_tft_raw_max[s][1]) { test_tft_raw_max[s][1] = ry; }
    if (TOUCH_SWAP_XY[s]) {
        const uint16_t t = rx; rx = ry; ry = t;
    }
    x = scale(rx, TOUCH_X_MIN[s], TOUCH_X_MAX[s], W);
    y = scale(ry, TOUCH_Y_MIN[s], TOUCH_Y_MAX[s], H);
    test_tft_xy[s][0] = (uint16_t)x;
    test_tft_xy[s][1] = (uint16_t)y;

    if (!finger[s]) {                         /* new touch */
        finger[s] = true;
        ++test_tft_touches[s];
        if (y < CANVAS_Y) {
            clear_canvas(s);
        }
    }
    if (y >= CANVAS_Y) {
        fill_rect(s, x - 3, y - 3, 7, 7, YELLOW);
        if (y - 3 < CANVAS_Y) {               /* keep the dot off the line above */
            fill_rect(s, BORDER, CANVAS_Y - 4, W - 2 * BORDER, 2, GREY);
        }
    }
    if (now - digits_ms[s] >= 150U) {         /* numbers: a few times a second is enough */
        digits_ms[s] = now;
        show_raw(s);
    }
}

/* ---------------- test ---------------- */

void TestTftTouch_Init(void)
{
    GPIO_InitTypeDef g = {0};
    uint8_t s;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    /* SPI1: SCK PA5, MOSI PB5 */
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF5_SPI1;
    g.Pin = GPIO_PIN_5;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_GPIO_Init(GPIOB, &g);
    SPI1->CR1 = 0;
    SPI1->CR2 = (7U << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH;           /* 8-bit frames */
    SPI1->CR1 = SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE                 /* one data line, transmit only */
              | SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI          /* master, CS by GPIO */
              | (TFT_SPI_BR << SPI_CR1_BR_Pos);                   /* CPOL 0, CPHA 0, MSB first */
    SPI1->CR1 |= SPI_CR1_SPE;

    /* outputs, idle high: screen CS/DC/RESET, touch CS; idle low: touch CLK/DIN */
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = 0;
    for (s = 0; s < 2U; ++s) {
        const Pin out[4] = { tft_cs[s], tft_dc[s], tft_rst[s], t_cs[s] };
        uint8_t i;
        for (i = 0; i < 4U; ++i) {
            put(out[i], true);
            g.Pin = out[i].pin;
            HAL_GPIO_Init(out[i].port, &g);
        }
    }
    put(t_clk, false);
    put(t_mosi, false);
    g.Pin = t_clk.pin | t_mosi.pin;
    HAL_GPIO_Init(GPIOE, &g);
    g.Mode = GPIO_MODE_INPUT;
    g.Pin = t_miso.pin;
    HAL_GPIO_Init(GPIOE, &g);
    g.Pull = GPIO_PULLUP;                                         /* T_IRQ is open drain */
    g.Pin = t_irq[0].pin | t_irq[1].pin;
    HAL_GPIO_Init(GPIOE, &g);

    touch_pause = SystemCoreClock / 4000000U;   /* >= 2 cycles each: >= 0.5 us, clock <= 1 MHz */
    for (s = 0; s < 2U; ++s) {
        test_tft_raw_min[s][0] = test_tft_raw_min[s][1] = 4095U;
        test_tft_raw_max[s][0] = test_tft_raw_max[s][1] = 0U;
    }
}

void TestTftTouch_Loop(void)
{
    const uint32_t now = HAL_GetTick();
    uint8_t s;

    if (!screens_ready) {                     /* first call: start and draw both screens (~1.5 s) */
        for (s = 0; s < 2U; ++s) {
            tft_init(s);
            draw_layout(s);
            clear_canvas(s);
            /* The XPT2046 only drives T_IRQ after it has received a command
             * with PD1 PD0 = 00, so send one before waiting for a touch. */
            (void)touch_read(s, 0xD0);
        }
        screens_ready = true;
    }
    if (now - last_ms < 10U) {                /* read the touch panels every 10 ms */
        return;
    }
    last_ms = now;
    for (s = 0; s < 2U; ++s) {
        poll_touch(s, now);
    }
    if (now - z_ms >= 100U) {                 /* pressure, read without T_IRQ: rises when pressed */
        z_ms = now;
        for (s = 0; s < 2U; ++s) {
            test_tft_z1[s] = touch_read(s, 0xB0);
        }
    }
}
