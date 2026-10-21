/* Module 4 (driver): 2x ILI9341 on shared SPI1, portrait 240x320.
 * Pins (PINOUT.md): display 0 = CS/DC/RST, display 1 = CS2/DC2/RST2.
 *
 * Frames are sent from gfx_fb with DMA in the background. Only the 16-row
 * bands that changed are sent (tft_ui.c decides which). Because both
 * displays share gfx_fb and the SPI bus, nothing may draw into gfx_fb or
 * start another transfer until Tft_IsBusy() is false.
 */
#ifndef TFT_H
#define TFT_H

#include <stdbool.h>
#include <stdint.h>

void Tft_Init(uint8_t display);                 /* blocking, at startup */
/* Start sending the bands set in band_mask (bit i = rows 16i..16i+15 of
 * gfx_fb). Returns false (nothing started) if a transfer is still running. */
bool Tft_SendBands(uint8_t display, uint32_t band_mask);
bool Tft_IsBusy(void);
/* Diagnostic: fill the whole display with one color using plain blocking
 * SPI (no DMA, no framebuffer). color is byte-swapped RGB565 (Gfx_To565). */
void Tft_FillBlocking(uint8_t display, uint16_t color);

#endif
