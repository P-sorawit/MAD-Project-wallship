#include "gfx.h"

#include <string.h>

/* 32-byte aligned so the Cortex-M7 D-cache can be cleaned by line for DMA. */
uint16_t gfx_fb[GFX_H][GFX_W] __attribute__((aligned(32)));

uint16_t Gfx_To565(Color c)
{
    const uint16_t v = (uint16_t)(((c >> 8) & 0xF800U) | ((c >> 5) & 0x07E0U) |
                                  ((c >> 3) & 0x001FU));
    return (uint16_t)((v >> 8) | (v << 8)); /* big-endian for SPI */
}

uint32_t Gfx_BandHash(int band)
{
    const uint16_t *px = &gfx_fb[band * GFX_BAND_H][0];
    uint32_t h = 2166136261U;
    int i;
    for (i = 0; i < GFX_BAND_H * GFX_W; ++i) {
        h = (h ^ px[i]) * 16777619U;
    }
    return h;
}

static int channel(Color c, int shift)
{
    return (int)((c >> shift) & 0xFFU);
}

Color Gfx_Mix(Color a, Color b, int num, int den)
{
    Color out = 0;
    int shift;
    if (den <= 0) {
        return a;
    }
    for (shift = 0; shift <= 16; shift += 8) {
        const int ca = channel(a, shift), cb = channel(b, shift);
        const int v = ca + (cb - ca) * num / den;
        out |= (Color)(v < 0 ? 0 : (v > 255 ? 255 : v)) << shift;
    }
    return out;
}

void Gfx_Pixel(int x, int y, Color c)
{
    if (x >= 0 && x < GFX_W && y >= 0 && y < GFX_H) {
        gfx_fb[y][x] = Gfx_To565(c);
    }
}

void Gfx_Rect(int x, int y, int w, int h, Color c)
{
    const uint16_t v = Gfx_To565(c);
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > GFX_W ? GFX_W : x + w;
    int y1 = y + h > GFX_H ? GFX_H : y + h;
    int i, j;
    for (j = y0; j < y1; ++j) {
        for (i = x0; i < x1; ++i) {
            gfx_fb[j][i] = v;
        }
    }
}

void Gfx_Frame(int x, int y, int w, int h, int t, Color c)
{
    Gfx_Rect(x, y, w, t, c);
    Gfx_Rect(x, y + h - t, w, t, c);
    Gfx_Rect(x, y, t, h, c);
    Gfx_Rect(x + w - t, y, t, h, c);
}

void Gfx_VGradient(int x, int y, int w, int h, Color top, Color bottom, int bands)
{
    int b;
    if (bands < 1) {
        bands = 1;
    }
    for (b = 0; b < bands; ++b) {
        const int y0 = y + h * b / bands, y1 = y + h * (b + 1) / bands;
        Gfx_Rect(x, y0, w, y1 - y0, Gfx_Mix(top, bottom, b, bands > 1 ? bands - 1 : 1));
    }
}

void Gfx_Line(int x0, int y0, int x1, int y1, Color c)
{
    const int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    const int dy = -(y1 > y0 ? y1 - y0 : y0 - y1), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        Gfx_Pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        if (2 * err >= dy) { err += dy; x0 += sx; }
        if (2 * err <= dx) { err += dx; y0 += sy; }
    }
}

void Gfx_Circle(int cx, int cy, int r, Color c)
{
    int x, y;
    for (y = -r; y <= r; ++y) {
        for (x = -r; x <= r; ++x) {
            if (x * x + y * y <= r * r + r) {
                Gfx_Pixel(cx + x, cy + y, c);
            }
        }
    }
}

void Gfx_Ring(int cx, int cy, int r, int t, Color c)
{
    const int ri = r - t;
    int x, y;
    for (y = -r; y <= r; ++y) {
        for (x = -r; x <= r; ++x) {
            const int d = x * x + y * y;
            if (d <= r * r + r && d > ri * ri + ri) {
                Gfx_Pixel(cx + x, cy + y, c);
            }
        }
    }
}

/* ---------------- text ---------------- */

int Gfx_TextWidth(const char *s, int scale, int gap)
{
    const int n = (int)strlen(s);
    return n == 0 ? 0 : n * 5 * scale + (n - 1) * gap;
}

typedef void (*PlotFn)(int x, int y, int scale, int row, void *ctx);

static void walk_text(int x, int y, const char *s, int scale, int gap,
                      PlotFn plot, void *ctx)
{
    for (; *s != '\0'; ++s, x += 5 * scale + gap) {
        const uint8_t *g = Font5x7_Glyph(*s);
        int col, row;
        for (col = 0; col < 5; ++col) {
            for (row = 0; row < 7; ++row) {
                if ((g[col] >> row) & 1U) {
                    plot(x + col * scale, y + row * scale, scale, row, ctx);
                }
            }
        }
    }
}

static void plot_solid(int x, int y, int scale, int row, void *ctx)
{
    (void)row;
    Gfx_Rect(x, y, scale, scale, *(const Color *)ctx);
}

void Gfx_Text(int x, int y, const char *s, int scale, int gap, Color c)
{
    walk_text(x, y, s, scale, gap, plot_solid, &c);
}

void Gfx_TextCenter(int cx, int y, const char *s, int scale, int gap, Color c)
{
    Gfx_Text(cx - Gfx_TextWidth(s, scale, gap) / 2, y, s, scale, gap, c);
}

typedef struct {
    Color top, bottom;
    int grow; /* outline thickness added on every side */
} TitleCtx;

static void plot_grown(int x, int y, int scale, int row, void *ctx)
{
    const TitleCtx *t = (const TitleCtx *)ctx;
    (void)row;
    Gfx_Rect(x - t->grow, y - t->grow, scale + 2 * t->grow, scale + 2 * t->grow, t->top);
}

static void plot_gradient(int x, int y, int scale, int row, void *ctx)
{
    const TitleCtx *t = (const TitleCtx *)ctx;
    /* top row gets a light highlight, then a 7-step gradient */
    const Color c = row == 0 ? Gfx_Mix(t->top, 0xFFFFFFU, 1, 2)
                             : Gfx_Mix(t->top, t->bottom, row, 6);
    Gfx_Rect(x, y, scale, scale, c);
}

void Gfx_TitleCenter(int cx, int y, const char *s, int scale, int gap,
                     Color top, Color bottom, Color outline, int outline_px)
{
    const int x = cx - Gfx_TextWidth(s, scale, gap) / 2;
    TitleCtx shadow = {0x000000U, 0x000000U, outline_px};
    TitleCtx border = {outline, outline, outline_px};
    TitleCtx fill = {top, bottom, 0};
    walk_text(x + outline_px, y + outline_px + 2, s, scale, gap, plot_grown, &shadow);
    walk_text(x, y, s, scale, gap, plot_grown, &border);
    walk_text(x, y, s, scale, gap, plot_gradient, &fill);
}
