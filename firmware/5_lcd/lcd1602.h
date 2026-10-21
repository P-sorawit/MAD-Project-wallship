/* Module 5 (driver): LCD1602 behind a PCF8574 I2C backpack, 4-bit mode.
 * Two displays share I2C1 (PB8 SCL, PB9 SDA):
 *   index 0 = P1 (address 0x27), index 1 = P2 (address 0x26).
 * Blocking I2C (~8 ms per 16-char line at 100 kHz), so call from the main
 * loop only, and only for lines that changed (lcd_ui.c does that).
 */
#ifndef LCD1602_H
#define LCD1602_H

#include <stdint.h>

void Lcd1602_Init(uint8_t lcd);
void Lcd1602_Clear(uint8_t lcd);
/* Writes exactly 16 characters; shorter text is padded with spaces. */
void Lcd1602_WriteLine(uint8_t lcd, uint8_t row, const char *text);

#endif
