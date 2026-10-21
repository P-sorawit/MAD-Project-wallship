#include "art.h"

const ShipPalette SHIP_STEEL = {0x26303CU, 0x46525FU, 0x75828FU, 0xA9B4BFU, 0xF6E7A1U};
const ShipPalette SHIP_NIGHT = {0x101318U, 0x1F252DU, 0x323A44U, 0x4A535EU, 0x6B5A3AU};
const ShipPalette SHIP_DUSK  = {0x1E1230U, 0x34204AU, 0x4D3366U, 0x6C4F86U, 0xFFC86AU};

/* Draw one art-unit rectangle. y grows upward from the waterline. */
static int g_x, g_y, g_u;
static void U(int ux, int uy, int uw, int uh, Color c)
{
    Gfx_Rect(g_x + ux * g_u, g_y - (uy + uh) * g_u, uw * g_u, uh * g_u, c);
}

/* Block with a lit top row, a body and a dark bottom row. */
static void block(int ux, int uy, int uw, int uh, const ShipPalette *p)
{
    U(ux, uy, uw, uh, p->mid);
    U(ux, uy + uh - 1, uw, 1, p->light);
    U(ux, uy, uw, 1, p->dark);
}

static void turret(int ux, int uy, int dir, const ShipPalette *p)
{
    block(ux, uy, 7, 3, p);
    U(ux + 1, uy + 3, 5, 1, p->light);
    /* two barrels */
    if (dir > 0) {
        U(ux + 7, uy + 2, 7, 1, p->dark);
        U(ux + 7, uy + 1, 6, 1, p->dark);
    } else {
        U(ux - 7, uy + 2, 7, 1, p->dark);
        U(ux - 6, uy + 1, 6, 1, p->dark);
    }
}

void Art_Battleship(int x, int water_y, int u, const ShipPalette *p)
{
    int r;
    g_x = x;
    g_y = water_y;
    g_u = u;

    /* Hull: 6 rows, stern rakes back a little, bow flares forward. */
    for (r = 0; r < 6; ++r) {
        const int left = 3 - r / 2;
        const int right = 64 + r + (r >= 4 ? r - 3 : 0);
        U(left, r, right - left, 1, r == 5 ? p->light : (r == 4 ? p->mid : p->dark));
    }
    U(1, 6, 70, 1, p->highlight);   /* deck edge */

    /* Turrets: one aft, two forward (second one raised). */
    turret(8, 7, -1, p);
    turret(46, 7, 1, p);
    block(38, 7, 6, 3, p);
    turret(37, 10, 1, p);

    /* Funnel */
    block(17, 7, 6, 8, p);
    U(17, 14, 6, 1, p->dark);
    U(18, 15, 4, 1, p->dark);

    /* Superstructure tiers + bridge windows */
    block(24, 7, 13, 4, p);
    block(26, 11, 9, 4, p);
    block(28, 15, 5, 3, p);
    for (r = 0; r < 4; ++r) {
        U(27 + r * 2, 12, 1, 1, p->window);
    }
    U(29, 16, 1, 1, p->window);
    U(31, 16, 1, 1, p->window);

    /* Mast, yard and flag */
    U(30, 18, 1, 8, p->dark);
    U(27, 22, 7, 1, p->dark);
    U(31, 24, 3, 2, 0xC8323AU);
}

void Art_Sea(int x, int y, int w, int h, Color top, Color bottom, Color wave, unsigned seed)
{
    int i;
    unsigned s = seed * 2654435761U + 12345U;
    Gfx_VGradient(x, y, w, h, top, bottom, 6);
    /* Short wave dashes, longer and lighter near the horizon. */
    for (i = 0; i < w * h / 160; ++i) {
        int wx, wy, len;
        s = s * 1103515245U + 12345U;
        wx = x + (int)((s >> 8) % (unsigned)w);
        s = s * 1103515245U + 12345U;
        wy = y + 2 + (int)((s >> 8) % (unsigned)(h - 3));
        len = 3 + (int)((s >> 20) % 7U);
        Gfx_Rect(wx, wy, len, 1, Gfx_Mix(wave, bottom, wy - y, h * 2));
        Gfx_Rect(wx + 1, wy + 1, len - 2, 1, Gfx_Mix(bottom, top, 1, 3));
    }
}

void Art_Birds(int x, int y, Color c)
{
    static const int pos[][2] = {{0, 4}, {10, 0}, {19, 6}, {28, 2}};
    int i;
    for (i = 0; i < 4; ++i) {
        const int bx = x + pos[i][0], by = y + pos[i][1];
        Gfx_Rect(bx, by, 2, 1, c);
        Gfx_Rect(bx + 2, by + 1, 1, 1, c);
        Gfx_Rect(bx + 3, by, 2, 1, c);
    }
}

void Art_Cloud(int x, int y, int u, Color c)
{
    Gfx_Rect(x + 2 * u, y, 6 * u, u, c);
    Gfx_Rect(x + u, y + u, 10 * u, u, c);
    Gfx_Rect(x, y + 2 * u, 14 * u, u, c);
}

void Art_Sun(int cx, int cy, int r, Color c, Color stripe)
{
    int i;
    Gfx_Circle(cx, cy, r, c);
    for (i = 0; i < 4; ++i) {
        Gfx_Rect(cx - r, cy + r / 4 + i * (r / 4 + 1), 2 * r + 1, 1 + i / 2, stripe);
    }
}

void Art_Firework(int cx, int cy, int r, Color c)
{
    static const int dir[12][2] = {
        {10, 0}, {9, 5}, {5, 9}, {0, 10}, {-5, 9}, {-9, 5},
        {-10, 0}, {-9, -5}, {-5, -9}, {0, -10}, {5, -9}, {9, -5}
    };
    int i, k;
    for (i = 0; i < 12; ++i) {
        for (k = 3; k <= 10; k += 2) {
            const int px = cx + dir[i][0] * r * k / 100, py = cy + dir[i][1] * r * k / 100;
            Gfx_Rect(px, py, k >= 9 ? 2 : 1, k >= 9 ? 2 : 1, k >= 9 ? 0xFFFFFFU : c);
        }
    }
    Gfx_Rect(cx - 1, cy - 1, 3, 3, 0xFFFFFFU);
}

void Art_Fire(int cx, int base_y, int u)
{
    static const int smoke[][3] = {
        {-6, 14, 7}, {-12, 22, 9}, {-4, 26, 8}, {-16, 32, 10}, {-8, 38, 9}, {-20, 44, 8}
    };
    static const int flame[][3] = {
        {-6, 3, 5}, {0, 6, 6}, {6, 3, 5}, {-3, 9, 4}, {3, 10, 4}, {0, 13, 3}
    };
    int i;
    for (i = 0; i < 6; ++i) {
        const Color sc = i % 2 ? 0x3C3C44U : 0x55555EU;
        Gfx_Circle(cx + smoke[i][0] * u, base_y - smoke[i][1] * u, smoke[i][2] * u, sc);
    }
    for (i = 0; i < 6; ++i) {
        Gfx_Circle(cx + flame[i][0] * u, base_y - flame[i][1] * u, flame[i][2] * u, 0xD7261EU);
    }
    for (i = 0; i < 6; ++i) {
        Gfx_Circle(cx + flame[i][0] * u, base_y - flame[i][1] * u - u, (flame[i][2] - 2) * u, 0xFF8A1EU);
    }
    Gfx_Circle(cx, base_y - 6 * u, 3 * u, 0xFFE14AU);
}

int Art_BoatWidth(int length, int s)
{
    return (8 + 10 * length) * s;
}

void Art_Boat(int x, int water_y, int length, int s, Color hull, Color top)
{
    const int w = 8 + 10 * length; /* in units of s pixels */
    const int cab = 6 + 2 * length;
#define B(ux, uy, uw, uh, c) Gfx_Rect(x + (ux) * s, water_y - (uy) * s, (uw) * s, (uh) * s, c)
    B(2, 2, w - 4, 2, Gfx_Mix(hull, 0, 1, 2));
    B(0, 5, w, 3, hull);
    B(1, 5, w - 2, 1, top);
    B(w / 2 - cab / 2, 9, cab, 4, top);
    B(w / 2 - cab / 2, 6, cab, 1, hull);
    if (length >= 2) {
        B(w / 2 - 1, 12, 2, 3, top);
    }
    if (length >= 3) {
        B(3, 7, 4, 2, top);          /* aft gun */
        B(w - 7, 7, 4, 2, top);      /* fore gun */
    }
#undef B
}

void Art_Crosshair(int cx, int cy, int r, Color c, Color dot)
{
    Gfx_Ring(cx, cy, r, 3, c);
    Gfx_Rect(cx - r - 5, cy - 1, r - 2, 3, c);
    Gfx_Rect(cx + 8, cy - 1, r - 2, 3, c);
    Gfx_Rect(cx - 1, cy - r - 5, 3, r - 2, c);
    Gfx_Rect(cx - 1, cy + 8, 3, r - 2, c);
    Gfx_Rect(cx - 2, cy - 2, 5, 5, dot);
}

void Art_Panel(int x, int y, int w, int h, Color fill, Color border)
{
    Gfx_Rect(x, y, w, h, fill);
    Gfx_Frame(x, y, w, h, 2, 0x05070EU);
    Gfx_Frame(x + 2, y + 2, w - 4, h - 4, 1, border);
}

/* ---------------- "go to the other side" pieces ---------------- */

void Art_Console(int x, int y)
{
    const Color wood = 0x8A5A34U, wood_dark = 0x5C3A20U;
    const Color body = 0x9AA3AEU, body_dark = 0x5E6670U, body_light = 0xC9D0D8U;
    int i, j;
    /* table: top, front edge, two legs */
    Gfx_Rect(x, y, 200, 6, wood);
    Gfx_Rect(x, y + 6, 200, 3, wood_dark);
    Gfx_Rect(x + 8, y + 9, 6, 28, wood_dark);
    Gfx_Rect(x + 186, y + 9, 6, 28, wood_dark);

    /* console base */
    Gfx_Rect(x + 20, y - 22, 160, 22, body);
    Gfx_Rect(x + 20, y - 22, 160, 3, body_light);
    Gfx_Rect(x + 20, y - 5, 160, 5, body_dark);
    /* key grids on both halves of the base top */
    for (j = 0; j < 2; ++j) {
        for (i = 0; i < 8; ++i) {
            Gfx_Rect(x + 26 + i * 8, y - 17 + j * 6, 6, 4, 0xF2F4F7U);
            Gfx_Rect(x + 112 + i * 8, y - 17 + j * 6, 6, 4, 0xF2F4F7U);
        }
    }
    /* divider standing in the middle, with the attack grid on its face */
    Gfx_Rect(x + 92, y - 74, 16, 54, body_dark);
    Gfx_Rect(x + 94, y - 72, 12, 50, body);
    for (j = 0; j < 7; ++j) {
        Gfx_Rect(x + 96, y - 69 + j * 7, 8, 5, 0xF2F4F7U);
    }
    /* small screens beside the divider: red = this side, green = other side */
    Gfx_Rect(x + 72, y - 34, 16, 10, 0x2A2E38U);
    Gfx_Rect(x + 74, y - 32, 12, 6, 0xC8323AU);
    Gfx_Rect(x + 112, y - 34, 16, 10, 0x2A2E38U);
    Gfx_Rect(x + 114, y - 32, 12, 6, 0x2DB84AU);
}

void Art_ArcArrow(int x0, int x1, int y, int rise, Color c)
{
    const int w = x1 - x0;
    int i;
    for (i = 0; i <= w; ++i) {
        /* parabola: 0 at both ends, -rise in the middle */
        const long t = (long)i * (w - i);
        const int yy = y - (int)(4L * rise * t / ((long)w * w));
        Gfx_Rect(x0 + i - 2, yy - 2, 5, 5, c);
    }
    /* arrow head pointing down at the right end */
    for (i = 0; i < 8; ++i) {
        Gfx_Rect(x1 - 8 + i, y - 2 + i, 2 * (8 - i), 2, c);
    }
}

void Art_Person(int x, int feet_y)
{
    const Color skin = 0xF0C29AU, hair = 0x4A2E1AU, shirt = 0x3C78C8U;
    const Color jacket = 0x8A5A34U, pants = 0x2A3446U, shoe = 0x1A1A22U;
    Gfx_Rect(x - 5, feet_y - 4, 6, 4, shoe);           /* shoes */
    Gfx_Rect(x + 2, feet_y - 4, 6, 4, shoe);
    Gfx_Rect(x - 4, feet_y - 22, 5, 18, pants);        /* legs (walking) */
    Gfx_Rect(x + 2, feet_y - 22, 5, 18, pants);
    Gfx_Rect(x - 7, feet_y - 42, 16, 21, jacket);      /* body */
    Gfx_Rect(x - 2, feet_y - 42, 6, 18, shirt);
    Gfx_Rect(x - 10, feet_y - 40, 4, 14, jacket);      /* arms */
    Gfx_Rect(x + 8, feet_y - 40, 4, 14, jacket);
    Gfx_Rect(x - 10, feet_y - 27, 4, 3, skin);
    Gfx_Rect(x + 8, feet_y - 27, 4, 3, skin);
    Gfx_Circle(x + 1, feet_y - 49, 6, skin);            /* head */
    Gfx_Rect(x - 5, feet_y - 56, 13, 4, hair);
    Gfx_Rect(x - 5, feet_y - 53, 3, 4, hair);
}
