#include "ws2812_test.h"

#include "ws2812.h"

#define SOLID_MS 1500U  /* patterns 1-5 */
#define WALK_MS  150U   /* pattern 6, per LED */

enum { PAT_RED = 1, PAT_GREEN, PAT_BLUE, PAT_WHITE, PAT_PANEL_ID, PAT_WALK, PAT_LAST = PAT_WALK };

static uint8_t pattern;
static uint8_t walk_index;
static uint32_t next_ms;
static uint8_t need_show;

static void draw(void)
{
    static const Rgb red = {255, 0, 0}, green = {0, 255, 0}, blue = {0, 0, 255};
    static const Rgb white = {255, 255, 255}, yellow = {255, 255, 0}, off = {0, 0, 0};
    uint8_t p;
    switch (pattern) {
    case PAT_RED:   Ws2812_SetAll(red);   break;
    case PAT_GREEN: Ws2812_SetAll(green); break;
    case PAT_BLUE:  Ws2812_SetAll(blue);  break;
    case PAT_WHITE: Ws2812_SetAll(white); break;
    case PAT_PANEL_ID: {
        static const uint8_t ch_to_panel[WS2812_CHANNELS] = WS2812_CHANNEL_TO_PANEL;
        const Rgb id_color[WS2812_CHANNELS] = {red, green, blue, yellow};
        for (p = 0; p < WS2812_CHANNELS; ++p) {
            Ws2812_SetPanel(ch_to_panel[p], id_color[p]); /* color = TIM1 channel */
        }
        break;
    }
    case PAT_WALK:
        Ws2812_SetAll(off);
        for (p = 0; p < WS2812_CHANNELS; ++p) {
            Ws2812_SetPixel(p, (uint8_t)(walk_index % BOARD_W),
                            (uint8_t)(walk_index / BOARD_W), white);
        }
        break;
    default:
        break;
    }
    need_show = 1;
}

void Ws2812Test_Init(void)
{
    Ws2812_Init();
    pattern = PAT_RED;
    walk_index = 0;
    next_ms = 0;
    need_show = 0;
}

void Ws2812Test_Loop(uint32_t now_ms)
{
    if (next_ms == 0 || (int32_t)(now_ms - next_ms) >= 0) {
        if (next_ms != 0) {
            /* advance */
            if (pattern == PAT_WALK && walk_index + 1U < WS2812_LEDS_PER_PANEL) {
                ++walk_index;
            } else {
                walk_index = 0;
                pattern = pattern == PAT_LAST ? (uint8_t)PAT_RED : (uint8_t)(pattern + 1U);
            }
        }
        draw();
        next_ms = now_ms + (pattern == PAT_WALK ? WALK_MS : SOLID_MS);
        if (next_ms == 0) {
            next_ms = 1;
        }
    }
    if (need_show && Ws2812_Show()) {
        need_show = 0; /* retried next call while DMA is busy */
    }
}

uint8_t Ws2812Test_Pattern(void)
{
    return pattern;
}
