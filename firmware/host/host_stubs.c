/* PC stand-ins for the HAL drivers so the HAL-free modules can be tested. */
#include "buzzer.h"
#include "ws2812.h"


int host_last_sound = -1;
unsigned host_ws_starts;

void Buzzer_Init(void) {}
void Buzzer_PlaySound(uint8_t mask, BuzzerSound s) { (void)mask; host_last_sound = (int)s; }
void Buzzer_Tick(uint32_t now_ms) { (void)now_ms; }
void Buzzer_SetMuted(uint8_t p, bool m) { (void)p; (void)m; }
bool Buzzer_IsMuted(uint8_t p) { (void)p; return false; }

bool Ws2812_Port_Start(uint8_t ch, const uint16_t *buf, uint32_t len)
{
    (void)buf; (void)len;
    ++host_ws_starts;
    Ws2812_OnChannelDone(ch); /* "DMA" finishes instantly */
    return true;
}

#include "tft.h"
#include "lcd1602.h"
#include <stdio.h>
char host_lcd[2][2][17];
unsigned host_lcd_writes;
void Lcd1602_Init(uint8_t lcd) { (void)lcd; }
void Lcd1602_Clear(uint8_t lcd) { (void)lcd; }
void Lcd1602_WriteLine(uint8_t lcd, uint8_t row, const char *text)
{
    snprintf(host_lcd[lcd][row], sizeof host_lcd[lcd][row], "%-16s", text);
    ++host_lcd_writes;
}
unsigned host_tft_blits;
uint32_t host_tft_last_mask;
void Tft_Init(uint8_t display) { (void)display; }
bool Tft_SendBands(uint8_t display, uint32_t band_mask)
{
    (void)display;
    host_tft_last_mask = band_mask;
    ++host_tft_blits;
    return true;            /* "DMA" finishes instantly */
}
bool Tft_IsBusy(void) { return false; }
