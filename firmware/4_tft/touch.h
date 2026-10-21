/* Module 4 (touch): 2x XPT2046, bit-banged like the lab ILI9341_Touchscreen
 * library. T_CLK / T_MOSI / T_MISO are shared; each panel has its own T_CS
 * and T_IRQ (EXTI). One tap = one event: a new tap is reported only after
 * the pen was lifted. */
#ifndef TOUCH_H
#define TOUCH_H

#include <stdbool.h>
#include <stdint.h>

void Touch_Init(void);
/* Main loop. True once per tap, with screen coordinates 0..239 / 0..319. */
bool Touch_Poll(uint8_t panel, int *x, int *y);
/* Last raw 12-bit reading, for calibration with the debugger. */
void Touch_LastRaw(uint8_t panel, uint16_t *raw_x, uint16_t *raw_y);

#endif
