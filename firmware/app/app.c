/* Main loop and interrupt routing. Interrupt handlers only capture input;
 * all game logic and drawing run in App_Loop().
 *
 *   TIM6 update (200 us)  -> Keypad_ScanIsr        (module 1)
 *   DMA2 / TIM1 pulse end -> Ws2812 channel done   (module 2)
 *   EXTI MUTEn            -> Buzzer_SetMuted       (instant silence)
 */
#include "main.h"
#include "app.h"

#include "app_config.h"
#include "buzzer.h"
#include "game.h"
#include "keypad.h"
#include "led_view.h"
#include "ws2812.h"

void Ws2812_Port_PulseFinished(TIM_HandleTypeDef *htim);

static Game game;
static bool leds_pending;

void App_Init(void)
{
    Buzzer_Init();
    Ws2812_Init();
    Keypad_Init();

    game.now_ms = HAL_GetTick();
    /* Seed: tick + uninitialised-ish timer noise. Mode START adds human timing. */
    Game_Init(&game, HAL_GetTick() ^ (uint32_t)SysTick->VAL ^ 0xA5A5A5A5U, GAME_AI_PROFILE);
    leds_pending = true;
}

void App_Loop(void)
{
    KeyEvent key;
    const uint32_t now = HAL_GetTick();

    /* 1. Inputs */
    while (Keypad_GetEvent(&key)) {
        Game_OnKey(&game, key.panel, key.x, key.y);
    }
    /* TODO: buttons (mode select, ship type, CANCEL/CONFIRM, SURRENDER,
     * NEW GAME) need a new input source now that the TFT is gone.
     * Whatever reads it should call App_OnButton(player, BTN_...). */

    /* 2. Time-based logic (S3 -> S4, AI shots, S6 -> S1) and sound */
    Game_Tick(&game, now);
    Buzzer_Tick(now);

    /* 3. Outputs, only when something changed */
    if (Game_TakeDirty(&game)) {
        LedView_Render(&game);
        leds_pending = true;
    }
    if (leds_pending && Ws2812_Show()) {
        leds_pending = false; /* retried next loop if DMA was still busy */
    }
}

void App_OnButton(uint8_t player, GameButton button)
{
    Game_OnButton(&game, player, button);
}

/* ---------------- HAL callbacks ---------------- */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    /* If CubeMX uses a TIMx as HAL timebase, main.c already defines this
     * callback: move this line into that one instead. */
    if (htim->Instance == TIM6) {
        Keypad_ScanIsr();
    }
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    Ws2812_Port_PulseFinished(htim);
}

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin == MUTE1_Pin) {
        Buzzer_SetMuted(0, HAL_GPIO_ReadPin(MUTE1_GPIO_Port, MUTE1_Pin) == GPIO_PIN_RESET);
    } else if (pin == MUTE2_Pin) {
        Buzzer_SetMuted(1, HAL_GPIO_ReadPin(MUTE2_GPIO_Port, MUTE2_Pin) == GPIO_PIN_RESET);
    }
}
