/* STM32 side of the sound module. CubeMX setup expected:
 *   TIM3: PSC so the counter runs at 1 MHz (PSC = 107 for a 108 MHz timer
 *         clock), CH1 = P1 buzzer, CH2 = P2 buzzer, PWM mode 1.
 *   MUTE1 / MUTE2: GPIO EXTI rising+falling, slide switch (see app.c).
 * Tone = 1 MHz / ARR, duty fixed at 50% (CCR = ARR / 2).
 */
#include "main.h"
#include "buzzer.h"

extern TIM_HandleTypeDef htim3;

typedef struct {
    uint16_t hz;  /* 0 = rest */
    uint16_t ms;
} Note;

#define END {0, 0}
static const Note sounds[SND_COUNT][5] = {
    [SND_CLICK]     = { {2000, 30}, END },
    [SND_ERROR]     = { {200, 120}, {0, 40}, {200, 120}, END },
    [SND_MISS]      = { {600, 80}, {400, 120}, END },
    [SND_HIT]       = { {1200, 80}, {1800, 120}, END },
    [SND_SUNK]      = { {1800, 100}, {1200, 100}, {800, 100}, {500, 250}, END },
    [SND_START]     = { {800, 100}, {1000, 100}, {1300, 200}, END },
    [SND_GAME_OVER] = { {1300, 150}, {1000, 150}, {800, 150}, {1600, 400}, END },
};

static const uint32_t channel[2] = { TIM_CHANNEL_1, TIM_CHANNEL_2 };
static volatile bool muted[2];
static uint8_t active_mask;
static const Note *seq;
static uint8_t seq_index;
static uint32_t note_end_ms;
static bool note_started;

static void output(uint16_t hz)
{
    uint8_t p;
    const uint32_t arr = hz != 0 ? (1000000U / hz) - 1U : 0U;
    if (hz != 0) {
        __HAL_TIM_SET_AUTORELOAD(&htim3, arr);
    }
    for (p = 0; p < 2; ++p) {
        const bool on = hz != 0 && (active_mask & (1U << p)) && !muted[p];
        __HAL_TIM_SET_COMPARE(&htim3, channel[p], on ? (arr + 1U) / 2U : 0U);
    }
}

void Buzzer_Init(void)
{
    muted[0] = HAL_GPIO_ReadPin(MUTE1_GPIO_Port, MUTE1_Pin) == GPIO_PIN_RESET; /* TODO: polarity */
    muted[1] = HAL_GPIO_ReadPin(MUTE2_GPIO_Port, MUTE2_Pin) == GPIO_PIN_RESET;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
}

void Buzzer_PlaySound(uint8_t mask, BuzzerSound s)
{
    if (s >= SND_COUNT) {
        return;
    }
    /* Only one tune at a time (shared ARR): the newest request wins. */
    seq = sounds[s];
    seq_index = 0;
    active_mask = mask;
    note_started = false;
}

void Buzzer_Tick(uint32_t now_ms)
{
    if (seq == NULL) {
        return;
    }
    if (note_started && (int32_t)(now_ms - note_end_ms) < 0) {
        return;
    }
    if (note_started) {
        ++seq_index;
    }
    if (seq_index >= 5 || seq[seq_index].ms == 0) {
        output(0);
        seq = NULL;
        return;
    }
    output(seq[seq_index].hz);
    note_end_ms = now_ms + seq[seq_index].ms;
    note_started = true;
}

void Buzzer_SetMuted(uint8_t p, bool m)
{
    if (p > 1) {
        return;
    }
    muted[p] = m;
    if (m) {
        __HAL_TIM_SET_COMPARE(&htim3, channel[p], 0); /* silence immediately */
    }
}

bool Buzzer_IsMuted(uint8_t p)
{
    return p <= 1 && muted[p];
}
