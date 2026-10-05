/* Sound: 2 passive buzzers on TIM3 CH1 (P1) / CH2 (P2), 50% duty.
 * Both channels share TIM3's ARR, so they always play the same pitch; a sound
 * is sent to a player mask instead of playing two different tunes at once.
 */
#ifndef BUZZER_H
#define BUZZER_H

#include <stdbool.h>
#include <stdint.h>

#define BUZZ_P1   0x01U
#define BUZZ_P2   0x02U
#define BUZZ_BOTH 0x03U
#define BUZZ_PLAYER(p) ((uint8_t)(1U << (p)))

typedef enum {
    SND_CLICK = 0,  /* accepted key/button */
    SND_ERROR,      /* rejected placement / repeated shot */
    SND_MISS,
    SND_HIT,
    SND_SUNK,
    SND_START,
    SND_GAME_OVER,
    SND_COUNT
} BuzzerSound;

void Buzzer_Init(void);
void Buzzer_PlaySound(uint8_t player_mask, BuzzerSound sound);
void Buzzer_Tick(uint32_t now_ms);          /* main loop: advance notes */
void Buzzer_SetMuted(uint8_t player, bool muted); /* safe from EXTI ISR */
bool Buzzer_IsMuted(uint8_t player);

#endif
