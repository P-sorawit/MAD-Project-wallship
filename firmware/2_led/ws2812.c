#include "ws2812.h"

#include <string.h>

static Rgb color[WS2812_CHANNELS][BOARD_W][BOARD_H];
uint16_t ws2812_dma[WS2812_CHANNELS][WS2812_DMA_LEN];
static volatile uint8_t busy_mask;

static const uint8_t channel_to_panel[WS2812_CHANNELS] = WS2812_CHANNEL_TO_PANEL;
static const uint8_t flip_x[WS2812_CHANNELS] = WS2812_FLIP_X;
static const uint8_t flip_y[WS2812_CHANNELS] = WS2812_FLIP_Y;

void Ws2812_Init(void)
{
    memset(color, 0, sizeof color);
    busy_mask = 0;
    Ws2812_Encode();
}

void Ws2812_SetPixel(uint8_t panel, uint8_t x, uint8_t y, Rgb c)
{
    if (panel < WS2812_CHANNELS && x < BOARD_W && y < BOARD_H) {
        color[panel][x][y] = c;
    }
}

void Ws2812_SetPanel(uint8_t panel, Rgb c)
{
    uint8_t x, y;
    for (x = 0; x < BOARD_W; ++x) {
        for (y = 0; y < BOARD_H; ++y) {
            Ws2812_SetPixel(panel, x, y, c);
        }
    }
}

void Ws2812_SetAll(Rgb c)
{
    uint8_t p;
    for (p = 0; p < WS2812_CHANNELS; ++p) {
        Ws2812_SetPanel(p, c);
    }
}

Rgb Ws2812_GetPixel(uint8_t panel, uint8_t x, uint8_t y)
{
    const Rgb off = {0, 0, 0};
    return (panel < WS2812_CHANNELS && x < BOARD_W && y < BOARD_H)
        ? color[panel][x][y] : off;
}

static uint16_t *put_byte(uint16_t *out, uint8_t value)
{
    /* Apply the brightness cap, then MSB first. */
    const uint8_t v = (uint8_t)((value * WS2812_BRIGHTNESS_CAP + 127U) / 255U);
    uint8_t bit;
    for (bit = 0x80U; bit != 0; bit >>= 1) {
        *out++ = (v & bit) ? WS2812_CCR_1 : WS2812_CCR_0;
    }
    return out;
}

void Ws2812_Encode(void)
{
    uint8_t ch;
    for (ch = 0; ch < WS2812_CHANNELS; ++ch) {
        const uint8_t panel = channel_to_panel[ch];
        uint16_t *out = ws2812_dma[ch];
        uint8_t i;
        for (i = 0; i < WS2812_LEDS_PER_PANEL; ++i) {
            /* Chain index i -> board cell (x, y). */
            uint8_t y = (uint8_t)(i / BOARD_W);
            uint8_t x = (uint8_t)(i % BOARD_W);
            Rgb c;
#if WS2812_SERPENTINE
            if (y & 1U) {
                x = (uint8_t)(BOARD_W - 1U - x);
            }
#endif
            if (flip_x[panel]) { x = (uint8_t)(BOARD_W - 1U - x); }
            if (flip_y[panel]) { y = (uint8_t)(BOARD_H - 1U - y); }
            c = color[panel][x][y];
            out = put_byte(out, c.g); /* WS2812B wire order is G, R, B */
            out = put_byte(out, c.r);
            out = put_byte(out, c.b);
        }
        /* Reset slots: CCR 0 keeps the line low after the last bit. */
        memset(out, 0, WS2812_RESET_SLOTS * sizeof *out);
    }
}

bool Ws2812_IsBusy(void)
{
    return busy_mask != 0;
}

bool Ws2812_Show(void)
{
    uint8_t ch;
    if (busy_mask != 0) {
        return false;
    }
    Ws2812_Encode();
    for (ch = 0; ch < WS2812_CHANNELS; ++ch) {
        if ((WS2812_ACTIVE_MASK & (1U << ch)) == 0U) {
            continue;
        }
        busy_mask |= (uint8_t)(1U << ch);
        if (!Ws2812_Port_Start(ch, ws2812_dma[ch], WS2812_DMA_LEN)) {
            busy_mask &= (uint8_t)~(1U << ch);
        }
    }
    return true;
}

void Ws2812_OnChannelDone(uint8_t channel)
{
    if (channel < WS2812_CHANNELS) {
        busy_mask &= (uint8_t)~(1U << channel);
    }
}
