/* TEST 3: buzzers (TIM3 CH1/CH2 PWM) + mute slide switches (EXTI).
 * Files: this + sound/buzzer.c
 * Plays all 7 game sounds, 1.5 s apart: first on P1 only, then P2 only,
 * then both, and repeats. Slide a mute switch: that side goes silent
 * immediately (inside the EXTI interrupt), the other side keeps playing.
 *
 * Watch: test_sound (0 CLICK, 1 ERROR, 2 MISS, 3 HIT, 4 SUNK, 5 START,
 *        6 GAME OVER), test_side (1 = P1, 2 = P2, 3 = both),
 *        test_muted[2], test_mute_edges[2] (+1 per switch movement = EXTI works)
 */
#include "main.h"
#include "hwtests.h"
#include "buzzer.h"

#define STEP_MS 1500U

volatile uint8_t test_sound;
volatile uint8_t test_side = BUZZ_P1;
volatile uint8_t test_muted[2];
volatile uint16_t test_mute_edges[2];
static uint32_t next_ms;

void TestBuzzer_Init(void)
{
    Buzzer_Init();
    test_sound = SND_COUNT - 1U;   /* first step wraps to CLICK on P1 */
    test_side = BUZZ_P1;
    next_ms = HAL_GetTick();
}

void TestBuzzer_Loop(void)
{
    const uint32_t now = HAL_GetTick();
    if ((int32_t)(now - next_ms) >= 0) {
        next_ms = now + STEP_MS;
        if (++test_sound >= SND_COUNT) {
            test_sound = 0;
            test_side = test_side == BUZZ_P1 ? BUZZ_P2
                      : (test_side == BUZZ_P2 ? BUZZ_BOTH : BUZZ_P1);
        }
        Buzzer_PlaySound(test_side, (BuzzerSound)test_sound);
    }
    Buzzer_Tick(now);
    test_muted[0] = (uint8_t)Buzzer_IsMuted(0);
    test_muted[1] = (uint8_t)Buzzer_IsMuted(1);
}

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin == MUTE1_Pin) {
        Buzzer_SetMuted(0, HAL_GPIO_ReadPin(MUTE1_GPIO_Port, MUTE1_Pin) == GPIO_PIN_RESET);
        ++test_mute_edges[0];
    } else if (pin == MUTE2_Pin) {
        Buzzer_SetMuted(1, HAL_GPIO_ReadPin(MUTE2_GPIO_Port, MUTE2_Pin) == GPIO_PIN_RESET);
        ++test_mute_edges[1];
    }
}
