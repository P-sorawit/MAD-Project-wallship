/* STM32 side of module 5. CubeMX: I2C1 Standard Mode 100 kHz on PB8/PB9.
 *
 * PCF8574 -> HD44780 wiring used by almost all backpacks:
 *   P0 RS, P1 RW, P2 EN, P3 backlight, P4..P7 = D4..D7
 * Give the second backpack address 0x26 by bridging its A0 jumper.
 */
#include "main.h"
#include "lcd1602.h"
#include "app_config.h"

extern I2C_HandleTypeDef hi2c1;

#define LCD_RS 0x01U
#define LCD_EN 0x04U
#define LCD_BL 0x08U

static const uint16_t addr[LCD_COUNT] = LCD_I2C_ADDR;

static void write_nibble(uint8_t lcd, uint8_t nibble, uint8_t rs)
{
    const uint8_t base = (uint8_t)((nibble & 0xF0U) | LCD_BL | rs);
    uint8_t buf[2];
    buf[0] = (uint8_t)(base | LCD_EN); /* data is latched on the EN falling edge */
    buf[1] = base;
    HAL_I2C_Master_Transmit(&hi2c1, addr[lcd], buf, 2, 10);
}

static void write_byte(uint8_t lcd, uint8_t value, uint8_t rs)
{
    write_nibble(lcd, (uint8_t)(value & 0xF0U), rs);
    write_nibble(lcd, (uint8_t)(value << 4), rs);
    /* One I2C transfer at 100 kHz (~200 us) is longer than the 37 us an
     * HD44780 command needs, so only clear/home need an extra delay. */
}

void Lcd1602_Init(uint8_t lcd)
{
    if (lcd >= LCD_COUNT) {
        return;
    }
    HAL_Delay(50);                                /* power-up */
    write_nibble(lcd, 0x30, 0); HAL_Delay(5);     /* force 8-bit mode x3 */
    write_nibble(lcd, 0x30, 0); HAL_Delay(1);
    write_nibble(lcd, 0x30, 0); HAL_Delay(1);
    write_nibble(lcd, 0x20, 0); HAL_Delay(1);     /* switch to 4-bit */
    write_byte(lcd, 0x28, 0);                     /* 2 lines, 5x8 font */
    write_byte(lcd, 0x0C, 0);                     /* display on, no cursor */
    write_byte(lcd, 0x06, 0);                     /* entry: increment */
    Lcd1602_Clear(lcd);
}

void Lcd1602_Clear(uint8_t lcd)
{
    if (lcd >= LCD_COUNT) {
        return;
    }
    write_byte(lcd, 0x01, 0);
    HAL_Delay(2);
}

void Lcd1602_WriteLine(uint8_t lcd, uint8_t row, const char *text)
{
    uint8_t i;
    if (lcd >= LCD_COUNT || row > 1U) {
        return;
    }
    write_byte(lcd, (uint8_t)(0x80U | (row ? 0x40U : 0x00U)), 0); /* DDRAM address */
    for (i = 0; i < 16U; ++i) {
        const char c = (text != NULL && *text != '\0') ? *text++ : ' ';
        write_byte(lcd, (uint8_t)c, LCD_RS);
    }
}
