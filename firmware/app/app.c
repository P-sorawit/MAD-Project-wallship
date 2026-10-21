/* app.c - glue between CubeMX main.c and the game modules.
 *
 * main.c calls App_Init() once and App_Loop() forever. Each loop pass:
 *
 *     read_inputs()      switch matrix keys + touch taps  -> game
 *     update_game()      timers (S3 -> S4, AI shots, S6 -> S1) + sound
 *     draw_outputs()     LEDs, LCDs, touch screens (only what changed)
 *
 * Interrupts only collect input, except two switches that must act at once
 * (proposal 1.2.3 #5):
 *     MUTE  -> silences that buzzer inside the interrupt
 *     SURR  -> ends the match inside the interrupt (see "surrender" below)
 *
 * Interrupt sources (course requirement: at least 2 modules, we use 3):
 *     TIM   TIM6 every 200 us            -> Keypad_ScanIsr()
 *     DMA   TIM1 DMA done (LED frame)    -> Ws2812_Port_PulseFinished()
 *           SPI1 DMA done (TFT image)    -> HAL_SPI_TxCpltCallback() in tft.c
 *     EXTI  SURR1/SURR2, MUTE1/MUTE2, T_IRQ/T_IRQ2 -> HAL_GPIO_EXTI_Callback()
 *
 * APP_HW_TEST 1 (app_config.h) replaces all of this with app/hwtest.c.
 */
#include "main.h"
#include "app.h"

#include "app_config.h"
#include "buzzer.h"
#include "game.h"
#include "hwtest.h"
#include "keypad.h"
#include "lcd1602.h"
#include "lcd_ui.h"
#include "led_view.h"
#include "tft.h"
#include "tft_ui.h"
#include "touch.h"
#include "ws2812.h"

void Ws2812_Port_PulseFinished(TIM_HandleTypeDef *htim);

#ifndef SURRENDER_DEBOUNCE_MS
#define SURRENDER_DEBOUNCE_MS 300U
#endif

static Game game;
#if !APP_HW_TEST
static bool leds_pending;      /* LED colors changed but not sent yet */
#endif

/* =========================================================================
 * Surrender (runs inside the EXTI interrupt)
 *
 * The interrupt may change `game` directly, except while the main loop is in
 * the middle of a Game_* call. The loop marks those moments with
 * game_enter()/game_leave(); a press that arrives then is parked and applied
 * in game_leave(). Drawing happens outside that window, so a surrender during
 * a screen redraw still ends the match immediately.
 * ========================================================================= */
static volatile bool game_busy;
static volatile bool surrender_pending[2];
static uint32_t surrender_last_ms[2];          /* used by the ISR only */

static void game_enter(void)
{
    game_busy = true;
}

static void game_leave(void)
{
    uint8_t p;
    for (p = 0; p < 2U; ++p) {
        if (surrender_pending[p]) {
            surrender_pending[p] = false;
            Game_OnButton(&game, p, BTN_SURRENDER);
        }
    }
    game_busy = false;
}

static void surrender_isr(uint8_t player)
{
    static GPIO_TypeDef *const port[2] = { SURR1_GPIO_Port, SURR2_GPIO_Port };
    static const uint16_t pin[2] = { SURR1_Pin, SURR2_Pin };
    const uint32_t now = HAL_GetTick();
    const bool bouncing = now - surrender_last_ms[player] < SURRENDER_DEBOUNCE_MS;
    const bool released = HAL_GPIO_ReadPin(port[player], pin[player]) != GPIO_PIN_RESET;

    if (bouncing || released) {
        return;
    }
    surrender_last_ms[player] = now;
#if APP_HW_TEST
    HwTest_OnSurrender(player);
#else
    if (game_busy) {
        surrender_pending[player] = true;              /* applied in game_leave() */
    } else {
        Game_OnButton(&game, player, BTN_SURRENDER);   /* ignored outside S3/S4 */
    }
#endif
}

static void mute_isr(uint8_t player, GPIO_TypeDef *port, uint16_t pin)
{
    const bool muted = HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET;  /* slid to GND */
    Buzzer_SetMuted(player, muted);
#if APP_HW_TEST
    HwTest_OnMute(player);
#endif
}

/* =========================================================================
 * Main loop steps
 * ========================================================================= */
#if !APP_HW_TEST

/* Keys from both players come through one queue, so both sides can place
 * ships at the same time. A tap can produce two commands (e.g. "1 player"
 * then "start"). */
static void read_inputs(void)
{
    KeyEvent key;
    uint8_t player;

    while (Keypad_GetEvent(&key)) {
        Game_OnKey(&game, key.panel, key.x, key.y);
    }
    for (player = 0; player < 2U; ++player) {
        int x, y;
        if (Touch_Poll(player, &x, &y)) {
            const TftAction tap = TftUi_HitTest(&game, player, x, y);
            if (tap.first != BTN_NONE) {
                Game_OnButton(&game, player, tap.first);
            }
            if (tap.second != BTN_NONE) {
                Game_OnButton(&game, player, tap.second);
            }
        }
    }
}

static void update_game(uint32_t now)
{
    Game_Tick(&game, now);
    Buzzer_Tick(now);
}

/* Outputs only read `game`. If a surrender interrupt changes it meanwhile,
 * it also sets the dirty flag, so the next pass redraws the final state. */
static void draw_outputs(bool game_changed)
{
    if (game_changed) {
        LedView_Render(&game);
        leds_pending = true;
        LcdUi_Render(&game);                 /* rewrites only changed lines */
    }
    if (leds_pending && Ws2812_Show()) {     /* false while DMA still busy */
        leds_pending = false;
    }
    TftUi_Render(&game);                     /* non-blocking, changed bands only */
}
#endif /* !APP_HW_TEST */

/* =========================================================================
 * Public API (called from main.c)
 * ========================================================================= */

void App_Init(void)
{
#if APP_HW_TEST
    HwTest_Init();
#else
    Buzzer_Init();
    Ws2812_Init();
    Tft_Init(0);
    Tft_Init(1);
    Touch_Init();
    TftUi_Init();
    Lcd1602_Init(0);
    Lcd1602_Init(1);
    LcdUi_Init();
    Keypad_Init();

    /* Random seed: tick + SysTick counter. Pressing START adds human timing. */
    game.now_ms = HAL_GetTick();
    Game_Init(&game, HAL_GetTick() ^ (uint32_t)SysTick->VAL ^ 0xA5A5A5A5U, GAME_AI_PROFILE);
    leds_pending = true;
#endif
}

void App_Loop(void)
{
#if APP_HW_TEST
    HwTest_Loop();
#else
    const uint32_t now = HAL_GetTick();
    bool game_changed;

    game_enter();
    read_inputs();
    update_game(now);
    game_changed = Game_TakeDirty(&game);    /* read + clear while guarded */
    game_leave();

    draw_outputs(game_changed);
#endif
}

void App_OnButton(uint8_t player, GameButton button)
{
    game_enter();
    Game_OnButton(&game, player, button);
    game_leave();
}

/* =========================================================================
 * HAL interrupt callbacks
 * ========================================================================= */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    /* If CubeMX uses a TIMx as HAL timebase, main.c already defines this
     * callback: move this call into that one instead. */
    if (htim->Instance == TIM6) {
        Keypad_ScanIsr();
    }
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    Ws2812_Port_PulseFinished(htim);
}

/* Touch pen-down. touch.c defines the real one (sets a "read me" flag);
 * this weak default only exists so app.c links without the touch module. */
__attribute__((weak)) void App_TouchIrq(uint8_t player)
{
    (void)player;
}

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin == SURR1_Pin) {
        surrender_isr(0);
    } else if (pin == SURR2_Pin) {
        surrender_isr(1);
    } else if (pin == MUTE1_Pin) {
        mute_isr(0, MUTE1_GPIO_Port, MUTE1_Pin);
    } else if (pin == MUTE2_Pin) {
        mute_isr(1, MUTE2_GPIO_Port, MUTE2_Pin);
    } else if (pin == T_IRQ_Pin) {
        App_TouchIrq(0);
    } else if (pin == T_IRQ2_Pin) {
        App_TouchIrq(1);
    }
}
