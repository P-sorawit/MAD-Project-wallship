/* Module 2: WS2812B driver, 4 panels x 56 LEDs on TIM1 CH1-CH4 + DMA2.
 *
 * Two buffers:
 *   color[panel][x][y] - what you edit (Ws2812_SetPixel / SetPanel / SetAll)
 *   dma[channel][bits] - CCR values (86 = bit 0, 173 = bit 1, ARR = 269),
 *                        rebuilt by Ws2812_Show() and sent by DMA.
 * ws2812.c is HAL-free; ws2812_port_stm32.c starts/stops the DMA.
 */
#ifndef WS2812_H
#define WS2812_H

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"

typedef struct {
    uint8_t r, g, b;
} Rgb;

#define WS2812_LEDS_PER_PANEL (BOARD_W * BOARD_H)               /* 56 */
#define WS2812_BITS_PER_LED   24U
#define WS2812_RESET_SLOTS    240U  /* 240 x 1.25us = 300us low (>280us) */
#define WS2812_DMA_LEN (WS2812_LEDS_PER_PANEL * WS2812_BITS_PER_LED + WS2812_RESET_SLOTS)

void Ws2812_Init(void);
void Ws2812_SetPixel(uint8_t panel, uint8_t x, uint8_t y, Rgb color);
void Ws2812_SetPanel(uint8_t panel, Rgb color);
void Ws2812_SetAll(Rgb color);
Rgb Ws2812_GetPixel(uint8_t panel, uint8_t x, uint8_t y);

/* Encode the color buffer into the DMA buffer and start all 4 channels.
 * Returns false (nothing sent) if the previous frame is still in flight;
 * the caller simply retries on the next loop.
 */
bool Ws2812_Show(void);
bool Ws2812_IsBusy(void);

/* Exposed for host tests and the port layer. */
extern uint16_t ws2812_dma[WS2812_CHANNELS][WS2812_DMA_LEN];
void Ws2812_Encode(void);
void Ws2812_OnChannelDone(uint8_t channel); /* called from DMA/PWM callback */

/* Implemented by the port layer (STM32 or host stub). */
bool Ws2812_Port_Start(uint8_t channel, const uint16_t *buf, uint32_t len);

#endif
