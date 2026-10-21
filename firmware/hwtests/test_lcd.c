/* TEST 5: LCD1602 x2 on I2C1 (PCF8574 backpacks).
 * Files: this + 5_lcd/lcd1602.c
 * At start: scans the I2C bus. Each LCD that answers at its configured
 * address (app_config.h LCD_I2C_ADDR: P1 0x27, P2 0x26) shows
 *   line 1: "LCD P1 ADDR 27"
 *   line 2: alternates every 2 s between "COUNT 12345" (seconds) and
 *           "ABCDEFGHIJKLMNOP" (all 16 columns lit)
 *
 * Watch: test_i2c_found[] / test_i2c_count  every address that answered
 *        test_lcd_ok[2]                      LCD found at its configured address
 */
#include "main.h"
#include "hwtests.h"
#include "app_config.h"
#include "lcd1602.h"

#include <stdio.h>

extern I2C_HandleTypeDef hi2c1;

volatile uint8_t test_i2c_found[16];
volatile uint8_t test_i2c_count;
volatile uint8_t test_lcd_ok[LCD_COUNT];
static uint32_t next_ms;
static uint32_t seconds;

void TestLcd_Init(void)
{
    static const uint16_t want[LCD_COUNT] = LCD_I2C_ADDR;
    uint8_t a, l;
    char line[17];
    /* PCF8574 answers at 0x20..0x27, PCF8574A at 0x38..0x3F */
    test_i2c_count = 0;
    for (a = 0x08; a < 0x78; ++a) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(a << 1), 2, 5) == HAL_OK &&
            test_i2c_count < sizeof test_i2c_found) {
            test_i2c_found[test_i2c_count++] = a;
        }
    }
    for (l = 0; l < LCD_COUNT; ++l) {
        test_lcd_ok[l] = 0;
        for (a = 0; a < test_i2c_count; ++a) {
            if ((uint16_t)(test_i2c_found[a] << 1) == want[l]) {
                test_lcd_ok[l] = 1;
            }
        }
        if (test_lcd_ok[l]) {
            Lcd1602_Init(l);
            snprintf(line, sizeof line, "LCD P%u ADDR %02X", (unsigned)l + 1U,
                     (unsigned)((want[l] >> 1) & 0xFFU));
            Lcd1602_WriteLine(l, 0, line);
        }
    }
    next_ms = HAL_GetTick();
}

void TestLcd_Loop(void)
{
    const uint32_t now = HAL_GetTick();
    char line[17];
    uint8_t l;
    if ((int32_t)(now - next_ms) < 0) {
        return;
    }
    next_ms = now + 1000U;
    ++seconds;
    if ((seconds / 2U) % 2U == 0U) {
        snprintf(line, sizeof line, "COUNT %lu", (unsigned long)(seconds % 100000UL));
    } else {
        snprintf(line, sizeof line, "ABCDEFGHIJKLMNOP");
    }
    for (l = 0; l < LCD_COUNT; ++l) {
        if (test_lcd_ok[l]) {
            Lcd1602_WriteLine(l, 1, line);
        }
    }
}
