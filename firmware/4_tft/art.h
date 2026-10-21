/* Module 4 (art): procedural pixel-art pieces for the touch screens.
 * Everything is drawn with gfx.h primitives, so no image files are needed. */
#ifndef ART_H
#define ART_H

#include "gfx.h"

typedef struct {
    Color dark, mid, light, highlight, window;
} ShipPalette;

extern const ShipPalette SHIP_STEEL;   /* day */
extern const ShipPalette SHIP_NIGHT;   /* defeat */
extern const ShipPalette SHIP_DUSK;    /* victory sunset */

/* Big battleship side view. (x, water_y) = stern at the waterline; `u` =
 * pixels per art unit (ship is 72 x 26 units). */
void Art_Battleship(int x, int water_y, int u, const ShipPalette *pal);

/* Sea from y to the bottom of the area, with wave highlights. */
void Art_Sea(int x, int y, int w, int h, Color top, Color bottom, Color wave, unsigned seed);

void Art_Birds(int x, int y, Color c);
void Art_Cloud(int x, int y, int u, Color c);
void Art_Sun(int cx, int cy, int r, Color c, Color stripe);
void Art_Firework(int cx, int cy, int r, Color c);
void Art_Fire(int cx, int base_y, int u);   /* flames + smoke */

/* Small boat icon, length 1..3 (fleet list / ship counter). */
/* `s` = pixel scale (1 = small counter icon, 2 = fleet list). */
int Art_BoatWidth(int length, int s);
void Art_Boat(int x, int water_y, int length, int s, Color hull, Color top);

void Art_Crosshair(int cx, int cy, int r, Color c, Color dot);

/* Retro panel: filled box with a light inner border and dark outer border. */
void Art_Panel(int x, int y, int w, int h, Color fill, Color border);

/* "Go to the other side" screen pieces (single-player, AI side). */
/* The game console on a table, seen from the side: base with key grid,
 * divider in the middle. (x, y) = top-left of the table top. */
void Art_Console(int x, int y);
/* Thick orange arc from (x0, y) to (x1, y), bulging up by `rise`, with an
 * arrow head at the right end. */
void Art_ArcArrow(int x0, int x1, int y, int rise, Color c);
/* Standing person, feet at (x, feet_y), about 52 px tall. */
void Art_Person(int x, int feet_y);

#endif
