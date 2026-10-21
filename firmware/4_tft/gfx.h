/* Module 4 (drawing): 240x320 RGB565 framebuffer + pixel-art primitives.
 * HAL-free. Screens are drawn here, then tft.c sends the whole buffer to one
 * display. Pixels are stored byte-swapped (big-endian) so the buffer can be
 * streamed to the ILI9341 over SPI as-is.
 */
#ifndef GFX_H_INCLUDED
#define GFX_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#define GFX_W 240
#define GFX_H 320

/* Partial updates: the screen is split into horizontal bands of 16 full
 * rows. A band is contiguous in gfx_fb, so DMA can send it as-is. */
#define GFX_BAND_H 16
#define GFX_BANDS  (GFX_H / GFX_BAND_H)   /* 20 bands, bit i = rows 16i..16i+15 */

/* Colors are written as 0xRRGGBB for readability. */
typedef uint32_t Color;

extern uint16_t gfx_fb[GFX_H][GFX_W];

uint16_t Gfx_To565(Color c);            /* byte-swapped RGB565 */
uint32_t Gfx_BandHash(int band);        /* FNV-1a of one band's pixels */
Color Gfx_Mix(Color a, Color b, int num, int den); /* a + (b-a)*num/den */

void Gfx_Pixel(int x, int y, Color c);
void Gfx_Rect(int x, int y, int w, int h, Color c);
void Gfx_Frame(int x, int y, int w, int h, int thick, Color c);
/* Vertical gradient split into `bands` flat steps (pixel-art look). */
void Gfx_VGradient(int x, int y, int w, int h, Color top, Color bottom, int bands);
void Gfx_Line(int x0, int y0, int x1, int y1, Color c);
void Gfx_Circle(int cx, int cy, int r, Color c);       /* filled */
void Gfx_Ring(int cx, int cy, int r, int thick, Color c);

/* Text uses a 5x7 font. `scale` multiplies each font pixel; `gap` is the
 * space between letters in screen pixels. */
int Gfx_TextWidth(const char *s, int scale, int gap);
void Gfx_Text(int x, int y, const char *s, int scale, int gap, Color c);
void Gfx_TextCenter(int cx, int y, const char *s, int scale, int gap, Color c);
/* Title style: vertical gradient fill, `outline` px dark border, drop shadow. */
void Gfx_TitleCenter(int cx, int y, const char *s, int scale, int gap,
                     Color top, Color bottom, Color outline, int outline_px);

/* Font: 5 column bytes, bit0 = top row. Special glyphs:
 *   '\x10' = right triangle (menu arrow), '\x11' = left triangle, '*' = x sign */
const uint8_t *Font5x7_Glyph(char c);

#endif
