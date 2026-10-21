/* STM32 side of module 4: ILI9341 x2 on SPI1 (PINOUT.md section 3), DMA.
 *
 * CubeMX:
 *   SPI1: Transmit Only Master (or Full-Duplex), 8-bit, CPOL Low, CPHA 1 Edge,
 *         MSB first, prescaler 4 (27 Mbit/s; use 8 or 16 if the picture is
 *         corrupted - the ILI9341 datasheet only guarantees 10 MHz).
 *   DMA:  SPI1_TX -> DMA2 Stream 5 (channel 3), Memory to Peripheral, Normal,
 *         Byte/Byte, memory increment. (Streams 1,2,4,6 are used by TIM1.)
 *   NVIC: DMA2 stream5 global interrupt on.
 *   GPIO outputs (initial High): CS, DC, RST, CS2, DC2, RST2.
 * TODO (on hardware): if the picture is mirrored/rotated, change TFT_MADCTL.
 *
 * Transfer state machine (runs in the DMA-complete interrupt):
 *   for each run of consecutive changed bands:
 *     set window (CASET/PASET/RAMWR, short blocking writes)
 *     stream the rows with DMA in chunks of <= 65280 bytes
 *   then raise CS and clear busy.
 */
#include "main.h"
#include "tft.h"
#include "gfx.h"

extern SPI_HandleTypeDef hspi1;

#define TFT_MADCTL 0x48U          /* portrait, column order mirrored, BGR panel */
#define DMA_CHUNK  65280U         /* even and < 65536 (HAL length is 16-bit) */

typedef struct {
    GPIO_TypeDef *cs_port;  uint16_t cs_pin;
    GPIO_TypeDef *dc_port;  uint16_t dc_pin;
    GPIO_TypeDef *rst_port; uint16_t rst_pin;
} TftPins;

static const TftPins pins[2] = {
    { CS_GPIO_Port,  CS_Pin,  DC_GPIO_Port,  DC_Pin,  RST_GPIO_Port,  RST_Pin },
    { CS2_GPIO_Port, CS2_Pin, DC2_GPIO_Port, DC2_Pin, RST2_GPIO_Port, RST2_Pin },
};

static volatile bool busy;
static uint8_t tx_display;
static uint32_t tx_mask;          /* bands still to start */
static const uint8_t *tx_ptr;     /* next byte of the current run */
static uint32_t tx_left;          /* bytes left in the current run */

static void tft_select(uint8_t d, GPIO_PinState level)
{
    HAL_GPIO_WritePin(pins[d].cs_port, pins[d].cs_pin, level);
}

static void command(uint8_t d, uint8_t cmd, const uint8_t *data, uint16_t len)
{
    HAL_GPIO_WritePin(pins[d].dc_port, pins[d].dc_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 10);
    HAL_GPIO_WritePin(pins[d].dc_port, pins[d].dc_pin, GPIO_PIN_SET);
    if (len != 0U) {
        HAL_SPI_Transmit(&hspi1, (uint8_t *)(uintptr_t)data, len, 10);
    }
}

/* Full ILI9341 power-up sequence, same as the lab driver that is known to
 * work with these panels (767_SPI_ILI9341_2, ILI9341_STM32_Driver.c).
 * Many clone panels stay white with only SLPOUT/DISPON. */
typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t data[15];
} InitCmd;

static const InitCmd init_seq[] = {
    {0xCB, 5,  {0x39, 0x2C, 0x00, 0x34, 0x02}},          /* power control A */
    {0xCF, 3,  {0x00, 0xC1, 0x30}},                      /* power control B */
    {0xE8, 3,  {0x85, 0x00, 0x78}},                      /* driver timing A */
    {0xEA, 2,  {0x00, 0x00}},                            /* driver timing B */
    {0xED, 4,  {0x64, 0x03, 0x12, 0x81}},                /* power on sequence */
    {0xF7, 1,  {0x20}},                                  /* pump ratio */
    {0xC0, 1,  {0x23}},                                  /* power control 1 */
    {0xC1, 1,  {0x10}},                                  /* power control 2 */
    {0xC5, 2,  {0x3E, 0x28}},                            /* VCM control 1 */
    {0xC7, 1,  {0x86}},                                  /* VCM control 2 */
    {0x36, 1,  {TFT_MADCTL}},                            /* memory access control */
    {0x3A, 1,  {0x55}},                                  /* 16-bit RGB565 */
    {0xB1, 2,  {0x00, 0x18}},                            /* frame rate */
    {0xB6, 3,  {0x08, 0x82, 0x27}},                      /* display function */
    {0xF2, 1,  {0x00}},                                  /* 3-gamma off */
    {0x26, 1,  {0x01}},                                  /* gamma curve 1 */
    {0xE0, 15, {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00}}, /* positive gamma */
    {0xE1, 15, {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F}}, /* negative gamma */
};

void Tft_Init(uint8_t d)
{
    size_t i;
    if (d > 1U) {
        return;
    }
    /* hardware reset (timings as in the lab driver) */
    tft_select(d, GPIO_PIN_SET);
    HAL_GPIO_WritePin(pins[d].rst_port, pins[d].rst_pin, GPIO_PIN_RESET);
    HAL_Delay(200);
    HAL_GPIO_WritePin(pins[d].rst_port, pins[d].rst_pin, GPIO_PIN_SET);
    HAL_Delay(200);

    tft_select(d, GPIO_PIN_RESET);
    command(d, 0x01, NULL, 0);        /* software reset */
    HAL_Delay(150);
    for (i = 0; i < sizeof init_seq / sizeof init_seq[0]; ++i) {
        command(d, init_seq[i].cmd, init_seq[i].data, init_seq[i].len);
    }
    command(d, 0x11, NULL, 0);        /* sleep out */
    HAL_Delay(120);
    command(d, 0x29, NULL, 0);        /* display on */
    tft_select(d, GPIO_PIN_SET);
}

static void send_chunk(void)
{
    const uint16_t n = (uint16_t)(tx_left > DMA_CHUNK ? DMA_CHUNK : tx_left);
    const uint8_t *p = tx_ptr;
    tx_ptr += n;
    tx_left -= n;
    HAL_SPI_Transmit_DMA(&hspi1, (uint8_t *)(uintptr_t)p, n);
}

/* Start the next run of consecutive bands; false when none are left. */
static bool start_run(void)
{
    static const uint8_t cols[4] = {0, 0, (GFX_W - 1) >> 8, (GFX_W - 1) & 0xFF};
    uint8_t rows[4];
    int first = 0, last;
    uint16_t y0, y1;
    if (tx_mask == 0U) {
        return false;
    }
    while (((tx_mask >> first) & 1U) == 0U) {
        ++first;
    }
    last = first;
    while (last + 1 < GFX_BANDS && ((tx_mask >> (last + 1)) & 1U) != 0U) {
        ++last;
    }
    tx_mask &= ~(((1UL << (last - first + 1)) - 1UL) << first);

    y0 = (uint16_t)(first * GFX_BAND_H);
    y1 = (uint16_t)((last + 1) * GFX_BAND_H - 1);
    rows[0] = (uint8_t)(y0 >> 8); rows[1] = (uint8_t)y0;
    rows[2] = (uint8_t)(y1 >> 8); rows[3] = (uint8_t)y1;
    command(tx_display, 0x2A, cols, 4);   /* full width */
    command(tx_display, 0x2B, rows, 4);   /* rows of this run */
    command(tx_display, 0x2C, NULL, 0);   /* memory write; DC stays high */

    tx_ptr = (const uint8_t *)&gfx_fb[y0][0];
    tx_left = (uint32_t)(y1 - y0 + 1U) * GFX_W * 2U;
    send_chunk();
    return true;
}

bool Tft_SendBands(uint8_t d, uint32_t band_mask)
{
    if (busy || d > 1U) {
        return false;
    }
    band_mask &= (1UL << GFX_BANDS) - 1UL;
    if (band_mask == 0U) {
        return true;
    }
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    /* Cortex-M7: DMA reads RAM, not the cache, so flush the framebuffer. */
    if (SCB->CCR & SCB_CCR_DC_Msk) {
        SCB_CleanDCache_by_Addr((uint32_t *)(uintptr_t)gfx_fb, (int32_t)sizeof gfx_fb);
    }
#endif
    busy = true;
    tx_display = d;
    tx_mask = band_mask;
    tft_select(d, GPIO_PIN_RESET);
    start_run();
    return true;
}

void Tft_FillBlocking(uint8_t d, uint16_t color)
{
    static const uint8_t cols[4] = {0, 0, (GFX_W - 1) >> 8, (GFX_W - 1) & 0xFF};
    static const uint8_t rows[4] = {0, 0, (GFX_H - 1) >> 8, (GFX_H - 1) & 0xFF};
    static uint16_t line[GFX_W];
    int i;
    if (busy || d > 1U) {
        return;
    }
    for (i = 0; i < GFX_W; ++i) {
        line[i] = color;
    }
    tft_select(d, GPIO_PIN_RESET);
    command(d, 0x2A, cols, 4);
    command(d, 0x2B, rows, 4);
    command(d, 0x2C, NULL, 0);
    for (i = 0; i < GFX_H; ++i) {
        HAL_SPI_Transmit(&hspi1, (uint8_t *)line, sizeof line, 10);
    }
    tft_select(d, GPIO_PIN_SET);
}

bool Tft_IsBusy(void)
{
    return busy;
}

/* DMA transfer complete (interrupt context). */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI1 || !busy) {
        return;
    }
    if (tx_left > 0U) {
        send_chunk();                 /* rest of this run */
    } else if (!start_run()) {        /* next run, or finished */
        tft_select(tx_display, GPIO_PIN_SET);
        busy = false;
    }
}
