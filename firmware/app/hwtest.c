#include "main.h"
#include "hwtest.h"

#include "app_config.h"
#include "buzzer.h"
#include "gfx.h"
#include "keypad.h"
#include "lcd1602.h"
#include "tft.h"
#include "touch.h"
#include "ws2812.h"
#include "ws2812_test.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

extern I2C_HandleTypeDef hi2c1;

static const char *const panel_name[4] = {"P1 FLEET", "P1 ATTACK", "P2 FLEET", "P2 ATTACK"};

/* I2C scan: PCF8574 = 0x20..0x27, PCF8574A = 0x38..0x3F */
static uint8_t found_addr[16];
static uint8_t found_count;
static bool lcd_ok[LCD_COUNT];

static bool key_mode;          /* first key press switches LEDs to key map */
static bool lit[4][BOARD_W][BOARD_H];
static KeyEvent last_key;
static uint16_t key_count[2];  /* per player side */
static bool leds_dirty;

static volatile uint16_t surr_count[2];
static volatile uint16_t mute_edges[2];

static int touch_x[2] = {-1, -1}, touch_y[2] = {-1, -1};
static uint16_t touch_raw[2][2];
static uint16_t touch_count[2];

static bool tft_dirty[2];
static bool lcd_dirty;
static uint32_t lcd_next_ms;
static uint16_t shown_surr[2], shown_mute[2];

void HwTest_OnSurrender(uint8_t p)
{
    if (p < 2U) {
        ++surr_count[p];
    }
}

void HwTest_OnMute(uint8_t p)
{
    if (p < 2U) {
        ++mute_edges[p];
    }
}

static void i2c_scan(void)
{
    static const uint8_t candidates[] = {
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
    };
    static const uint16_t want[LCD_COUNT] = LCD_I2C_ADDR;
    uint8_t i, l;
    found_count = 0;
    for (i = 0; i < sizeof candidates; ++i) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(candidates[i] << 1), 2, 5) == HAL_OK) {
            found_addr[found_count++] = candidates[i];
        }
    }
    for (l = 0; l < LCD_COUNT; ++l) {
        lcd_ok[l] = false;
        for (i = 0; i < found_count; ++i) {
            if ((uint16_t)(found_addr[i] << 1) == want[l]) {
                lcd_ok[l] = true;
            }
        }
    }
}

void HwTest_Init(void)
{
    uint8_t p;
    Buzzer_Init();
    Ws2812Test_Init();               /* also Ws2812_Init() */
    Tft_Init(0);
    Tft_Init(1);
    Touch_Init();
    i2c_scan();
    for (p = 0; p < LCD_COUNT; ++p) {
        if (lcd_ok[p]) {
            Lcd1602_Init(p);
        }
    }
    Keypad_Init();
    Buzzer_PlaySound(BUZZ_BOTH, SND_START);
    tft_dirty[0] = tft_dirty[1] = true;
    lcd_dirty = true;
}

/* ---------------- LEDs ---------------- */

static void draw_key_map(void)
{
    static const Rgb tint[4] = {{0, 0, 60}, {0, 50, 0}, {40, 0, 50}, {50, 40, 0}};
    static const Rgb on = {255, 255, 255}, last = {255, 0, 0};
    uint8_t p, x, y;
    for (p = 0; p < 4U; ++p) {
        for (x = 0; x < BOARD_W; ++x) {
            for (y = 0; y < BOARD_H; ++y) {
                Ws2812_SetPixel(p, x, y, lit[p][x][y] ? on : tint[p]);
            }
        }
    }
    Ws2812_SetPixel(last_key.panel, last_key.x, last_key.y, last);
}

/* ---------------- LCD ---------------- */

static void update_lcds(void)
{
    char l0[17], l1[17];
    uint8_t p;
    for (p = 0; p < LCD_COUNT; ++p) {
        if (!lcd_ok[p]) {
            continue;
        }
        snprintf(l0, sizeof l0, "LCD%u OK KEY%5u", (unsigned)p + 1U, (unsigned)(key_count[p] % 10000U));
        snprintf(l1, sizeof l1, "MUTE:%-3s SURR%3u", Buzzer_IsMuted(p) ? "ON" : "OFF",
                 (unsigned)(surr_count[p] % 1000U));
        Lcd1602_WriteLine(p, 0, l0);
        Lcd1602_WriteLine(p, 1, l1);
    }
}

/* ---------------- TFT ---------------- */

static void draw_tft(uint8_t p)
{
    static const uint16_t want[LCD_COUNT] = LCD_I2C_ADDR;
    char buf[40];
    int i, y = 52;
    Gfx_Rect(0, 0, GFX_W, GFX_H, 0x000000U);
    /* color bars: if red/blue are swapped, change MADCTL BGR bit */
    Gfx_Rect(0, 0, 48, 20, 0xFF0000U);
    Gfx_Rect(48, 0, 48, 20, 0x00FF00U);
    Gfx_Rect(96, 0, 48, 20, 0x0000FFU);
    Gfx_Rect(144, 0, 48, 20, 0xFFFFFFU);
    Gfx_Text(6, 6, "R", 1, 0, 0xFFFFFFU);
    Gfx_Text(54, 6, "G", 1, 0, 0x000000U);
    Gfx_Text(102, 6, "B", 1, 0, 0xFFFFFFU);
    Gfx_Text(150, 6, "W", 1, 0, 0x000000U);
    Gfx_Frame(0, 0, GFX_W, GFX_H, 1, 0xFFFFFFU);        /* full frame visible? */
    Gfx_Text(4, 24, "TOP LEFT", 1, 1, 0xFFE14AU);
    snprintf(buf, sizeof buf, "TFT P%u", (unsigned)p + 1U);
    Gfx_Text(130, 24, buf, 2, 1, 0x7FD6FFU);

    snprintf(buf, sizeof buf, "LCD1 %02X:%s", (unsigned)(want[0] >> 1), lcd_ok[0] ? "OK" : "MISSING");
    Gfx_Text(6, y, buf, 2, 1, lcd_ok[0] ? 0x9CFF8AU : 0xFF5A4AU);
    y += 20;
    snprintf(buf, sizeof buf, "LCD2 %02X:%s", (unsigned)(want[1] >> 1), lcd_ok[1] ? "OK" : "MISSING");
    Gfx_Text(6, y, buf, 2, 1, lcd_ok[1] ? 0x9CFF8AU : 0xFF5A4AU);
    y += 20;
    strcpy(buf, "I2C:");
    for (i = 0; i < found_count && i < 6; ++i) {
        char h[4];
        snprintf(h, sizeof h, " %02X", (unsigned)found_addr[i]);
        strcat(buf, h);
    }
    if (found_count == 0U) {
        strcat(buf, " NONE");
    }
    Gfx_Text(6, y, buf, 2, 1, 0xFFFFFFU);
    y += 28;

    snprintf(buf, sizeof buf, "KEYS P1:%u P2:%u", (unsigned)(key_count[0] % 1000U),
             (unsigned)(key_count[1] % 1000U));
    Gfx_Text(6, y, buf, 2, 1, 0xFFFFFFU);
    y += 20;
    if (key_mode) {
        snprintf(buf, sizeof buf, "%s %c%u", panel_name[last_key.panel % 4U],
                 'A' + last_key.x % 8U, (unsigned)(last_key.y % 8U) + 1U);
    } else {
        snprintf(buf, sizeof buf, "PRESS ANY KEY");
    }
    Gfx_Text(6, y, buf, 2, 1, 0xFF9A2EU);
    y += 28;

    snprintf(buf, sizeof buf, "MUTE%u:%s (%u)", (unsigned)p + 1U, Buzzer_IsMuted(p) ? "ON" : "OFF",
             (unsigned)(mute_edges[p] % 1000U));
    Gfx_Text(6, y, buf, 2, 1, 0xFFFFFFU);
    y += 20;
    snprintf(buf, sizeof buf, "SURR%u:%u", (unsigned)p + 1U, (unsigned)(surr_count[p] % 1000U));
    Gfx_Text(6, y, buf, 2, 1, 0xFFFFFFU);
    y += 28;

    snprintf(buf, sizeof buf, "TOUCH #%u", (unsigned)(touch_count[p] % 1000U));
    Gfx_Text(6, y, buf, 2, 1, 0x7FD6FFU);
    y += 20;
    snprintf(buf, sizeof buf, "RAW %4u %4u", (unsigned)touch_raw[p][0], (unsigned)touch_raw[p][1]);
    Gfx_Text(6, y, buf, 2, 1, 0x7FD6FFU);
    y += 20;
    snprintf(buf, sizeof buf, "XY  %3d %3d", touch_x[p], touch_y[p]);
    Gfx_Text(6, y, buf, 2, 1, 0x7FD6FFU);
    Gfx_Text(6, GFX_H - 12, "TAP THE 4 CORNERS TO CALIBRATE", 1, 1, 0x6E7682U);

    if (touch_x[p] >= 0) {                       /* where the panel thinks you tapped */
        Gfx_Rect(touch_x[p] - 8, touch_y[p], 17, 1, 0xFF3A2EU);
        Gfx_Rect(touch_x[p], touch_y[p] - 8, 1, 17, 0xFF3A2EU);
    }
}

/* ---------------- loop ---------------- */

void HwTest_Loop(void)
{
    const uint32_t now = HAL_GetTick();
    KeyEvent e;
    uint8_t p;

    while (Keypad_GetEvent(&e)) {
        const uint8_t side = (uint8_t)(e.panel / 2U);
        key_mode = true;
        last_key = e;
        lit[e.panel][e.x][e.y] = !lit[e.panel][e.x][e.y];
        ++key_count[side];
        Buzzer_PlaySound(BUZZ_PLAYER(side), SND_CLICK);
        leds_dirty = true;
        lcd_dirty = true;
        tft_dirty[0] = tft_dirty[1] = true;
    }
    for (p = 0; p < 2U; ++p) {
        int x, y;
        if (Touch_Poll(p, &x, &y)) {
            touch_x[p] = x;
            touch_y[p] = y;
            Touch_LastRaw(p, &touch_raw[p][0], &touch_raw[p][1]);
            ++touch_count[p];
            Buzzer_PlaySound(BUZZ_PLAYER(p), SND_CLICK);
            tft_dirty[p] = true;
        }
        if (surr_count[p] != shown_surr[p]) {
            shown_surr[p] = surr_count[p];
            Buzzer_PlaySound(BUZZ_PLAYER(p), SND_SUNK);
            lcd_dirty = true;
            tft_dirty[p] = true;
        }
        if (mute_edges[p] != shown_mute[p]) {
            shown_mute[p] = mute_edges[p];
            lcd_dirty = true;
            tft_dirty[p] = true;
        }
    }

    if (!key_mode) {
        Ws2812Test_Loop(now);
    } else if (leds_dirty && !Ws2812_IsBusy()) {
        draw_key_map();
        Ws2812_Show();
        leds_dirty = false;
    }

    Buzzer_Tick(now);

    if (lcd_dirty && (int32_t)(now - lcd_next_ms) >= 0) {
        update_lcds();
        lcd_dirty = false;
        lcd_next_ms = now + 200U;                /* LCD over I2C is slow */
    }

    if (!Tft_IsBusy()) {
        for (p = 0; p < 2U; ++p) {
            if (tft_dirty[p]) {
                tft_dirty[p] = false;
                draw_tft(p);
                Tft_SendBands(p, (1UL << GFX_BANDS) - 1UL);
                break;                           /* one display per loop */
            }
        }
    }
}
