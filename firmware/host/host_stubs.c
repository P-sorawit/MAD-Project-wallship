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
