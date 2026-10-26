/* TEST 5b: two LCD1602 with I2C backpacks (PCF8574), stand-alone.
 * No other file and no CubeMX setting needed: PB8/PB9 are driven here as a
 * software I2C bus (the game later uses hardware I2C1 on the same pins).
 * Defines no HAL callback, so it can sit in the build next to another test.
 *
 * Wiring, both LCDs on the same 4 wires (in parallel):
 *   GND -> CN8-11 GND        VCC -> CN8-9 +5V (USB is enough for 2 LCDs)
 *   SDA -> PB9 CN7-4         SCL -> PB8 CN7-2
 * The backpack has the pull-ups (to 5 V); PB8/PB9 are 5 V tolerant.
 * Addresses: LCD P1 = 0x27 (A0 A1 A2 open), LCD P2 = 0x26 (bridge A0 only).
 * PCF8574A backpacks answer at 0x38-0x3F instead; those are shown as "??".
 *
 * Each LCD that answers shows
 *   line 1  "LCD P1  ADDR 27"   (P2 for 0x26)
 *   line 2  every 2 s: ABCDEFGHIJKLMNOP -> 16 full blocks -> "0123456789 T0042"
 * The bus is checked every second, so an LCD powered or plugged in later
 * shows up by itself (still: power off before plugging wires).
 *
 * Live Expressions:
 *   test_lcd_bus_ok     0 = SDA or SCL held low (no 5 V on the backpack, short, no pull-up)
 *   test_lcd_count      LCDs answering;  test_lcd_addr[0..3] their addresses
 *   test_lcd_nack       writes not acknowledged (loose wire / noise), should stay 0
 *   test_lcd_seconds    counts up while the test runs
 */
#include "main.h"
#include "hwtests.h"
#include <stdbool.h>

#define SCL_PORT    GPIOB
#define SCL_PIN     GPIO_PIN_8
#define SDA_PORT    GPIOB
#define SDA_PIN     GPIO_PIN_9
#define HALF_BIT_US 5U          /* <= 100 kHz */

#define LCD_RS 0x01U            /* PCF8574 P0..P3 = RS, RW, EN, backlight; P4..P7 = D4..D7 */
#define LCD_EN 0x04U
#define LCD_BL 0x08U

/* PCF8574: 0x20-0x27, PCF8574A: 0x38-0x3F */
#define CANDIDATES 16U
static uint8_t candidate(uint8_t i) { return (uint8_t)(i < 8U ? 0x20U + i : 0x38U + i - 8U); }

volatile uint8_t  test_lcd_bus_ok;
volatile uint8_t  test_lcd_count;
volatile uint8_t  test_lcd_addr[4];
volatile uint16_t test_lcd_nack;
volatile uint32_t test_lcd_seconds;

static bool present[CANDIDATES];
static uint32_t last_ms;

/* ---------------- software I2C (open drain: SET = released, line pulled high) ---------------- */

static void delay_us(uint32_t us)
{
    volatile uint32_t n = us * (SystemCoreClock / 1000000U) / 2U;   /* >= 2 cycles per pass */
    while (n-- != 0U) {
    }
}

static void scl(bool high)
{
    HAL_GPIO_WritePin(SCL_PORT, SCL_PIN, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
    delay_us(HALF_BIT_US);
}

static void sda(bool high)
{
    HAL_GPIO_WritePin(SDA_PORT, SDA_PIN, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
    delay_us(HALF_BIT_US);
}

static bool sda_high(void) { return HAL_GPIO_ReadPin(SDA_PORT, SDA_PIN) == GPIO_PIN_SET; }
static bool scl_high(void) { return HAL_GPIO_ReadPin(SCL_PORT, SCL_PIN) == GPIO_PIN_SET; }

static void i2c_start(void) { sda(true); scl(true); sda(false); scl(false); }
static void i2c_stop(void)  { sda(false); scl(true); sda(true); }

static bool i2c_write(uint8_t value)
{
    uint8_t bit;
    bool ack;
    for (bit = 0x80U; bit != 0U; bit >>= 1) {
        sda((value & bit) != 0U);
        scl(true);
        scl(false);
    }
    sda(true);                  /* release SDA: the device pulls it low to acknowledge */
    scl(true);
    ack = !sda_high();
    scl(false);
    return ack;
}

/* Both lines must idle high. A device stuck mid-byte is freed with up to 9 clocks. */
static bool bus_free(void)
{
    uint8_t i;
    sda(true);
    scl(true);
    for (i = 0; i < 9U && !sda_high(); ++i) {
        scl(false);
        scl(true);
    }
    if (sda_high() && scl_high()) {
        i2c_stop();
        return true;
    }
    return false;
}

static bool probe(uint8_t addr)
{
    bool ack;
    i2c_start();
    ack = i2c_write((uint8_t)(addr << 1));
    i2c_stop();
    return ack;
}

static void pcf_write2(uint8_t addr, uint8_t a, uint8_t b)
{
    bool ok;
    i2c_start();
    ok = i2c_write((uint8_t)(addr << 1)) && i2c_write(a) && i2c_write(b);
    i2c_stop();
    if (!ok) {
        ++test_lcd_nack;
    }
}

/* ---------------- HD44780 in 4-bit mode behind the PCF8574 ---------------- */

static void lcd_nibble(uint8_t addr, uint8_t nibble, uint8_t rs)
{
    const uint8_t base = (uint8_t)((nibble & 0xF0U) | LCD_BL | rs);
    pcf_write2(addr, (uint8_t)(base | LCD_EN), base);   /* latched on the EN falling edge */
}

static void lcd_byte(uint8_t addr, uint8_t value, uint8_t rs)
{
    lcd_nibble(addr, (uint8_t)(value & 0xF0U), rs);
    lcd_nibble(addr, (uint8_t)(value << 4), rs);
}

static void lcd_init(uint8_t addr)
{
    HAL_Delay(50);                                  /* LCD power-up */
    lcd_nibble(addr, 0x30, 0); HAL_Delay(5);        /* force 8-bit mode 3 times */
    lcd_nibble(addr, 0x30, 0); HAL_Delay(1);
    lcd_nibble(addr, 0x30, 0); HAL_Delay(1);
    lcd_nibble(addr, 0x20, 0); HAL_Delay(1);        /* then 4-bit mode */
    lcd_byte(addr, 0x28, 0);                        /* 2 lines, 5x8 font */
    lcd_byte(addr, 0x0C, 0);                        /* display on, no cursor */
    lcd_byte(addr, 0x06, 0);                        /* cursor moves right */
    lcd_byte(addr, 0x01, 0);                        /* clear */
    HAL_Delay(2);
}

static void lcd_line(uint8_t addr, uint8_t row, const char *text)
{
    uint8_t i;
    lcd_byte(addr, (uint8_t)(row ? 0xC0U : 0x80U), 0);   /* cursor to start of the row */
    for (i = 0; i < 16U; ++i) {
        lcd_byte(addr, (uint8_t)text[i], LCD_RS);
    }
}

/* ---------------- test ---------------- */

static char hex_digit(uint8_t v) { return "0123456789ABCDEF"[v & 0x0FU]; }

static void show_title(uint8_t addr)
{
    char line[17] = "LCD ??  ADDR ?? ";
    if (addr == 0x27U) { line[4] = 'P'; line[5] = '1'; }
    if (addr == 0x26U) { line[4] = 'P'; line[5] = '2'; }
    line[13] = hex_digit((uint8_t)(addr >> 4));
    line[14] = hex_digit((uint8_t)(addr & 0x0FU));
    lcd_line(addr, 0, line);
}

static void show_pattern(uint8_t addr, uint32_t seconds)
{
    char line[17] = "0123456789 T0000";
    uint8_t i;
    switch ((seconds / 2U) % 3U) {
    case 0:
        lcd_line(addr, 1, "ABCDEFGHIJKLMNOP");       /* every column shows a character */
        break;
    case 1:
        for (i = 0; i < 16U; ++i) { line[i] = (char)0xFF; }   /* full block: every pixel on */
        lcd_line(addr, 1, line);
        break;
    default:
        for (i = 0; i < 4U; ++i) {                            /* seconds, last 4 digits */
            line[15 - i] = (char)('0' + seconds % 10U);
            seconds /= 10U;
        }
        lcd_line(addr, 1, line);
        break;
    }
}

void TestLcd16x2_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(SCL_PORT, SCL_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SDA_PORT, SDA_PIN, GPIO_PIN_SET);
    g.Pin = SCL_PIN | SDA_PIN;
    g.Mode = GPIO_MODE_OUTPUT_OD;
    g.Pull = GPIO_NOPULL;               /* the backpack pulls up to 5 V */
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SCL_PORT, &g);
    last_ms = HAL_GetTick() - 1000U;    /* first check right away */
}

void TestLcd16x2_Loop(void)
{
    const uint32_t now = HAL_GetTick();
    uint8_t i, count = 0;

    if (now - last_ms < 1000U) {
        return;
    }
    last_ms = now;
    ++test_lcd_seconds;

    test_lcd_bus_ok = bus_free();
    for (i = 0; i < CANDIDATES; ++i) {
        const uint8_t addr = candidate(i);
        const bool was = present[i];
        present[i] = test_lcd_bus_ok && probe(addr);
        if (!present[i]) {
            continue;
        }
        if (count < sizeof test_lcd_addr) {
            test_lcd_addr[count] = addr;
        }
        ++count;
        if (!was) {                     /* new or powered again: set it up */
            lcd_init(addr);
            show_title(addr);
        }
        show_pattern(addr, test_lcd_seconds);
    }
    for (i = count; i < sizeof test_lcd_addr; ++i) {
        test_lcd_addr[i] = 0;
    }
    test_lcd_count = count;
}
