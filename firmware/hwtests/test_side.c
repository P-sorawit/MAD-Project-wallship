/* TEST 3b: side controls of both players - surrender buttons, mute slide
 * switches and buzzers - stand-alone (no other file).
 *
 * No CubeMX setting needed: TIM3 and the 6 pins are set up here; the
 * switches are polled (no EXTI, no interrupt, no HAL callback), so it can
 * sit in the build next to the other tests.
 *
 * Wiring (= PINOUT.md):                 P1               P2
 *   buzzer (1k -> 2N2222 base)          PC6 CN7-1        PC7 CN7-11
 *   surrender button (other leg GND)    PE10 CN10-24     PE12 CN10-26
 *   mute slide switch (middle leg;      PE0 CN10-33      PB1 CN10-7
 *     one side leg to GND)
 *
 * What you hear
 *   start         P1 buzzer beeps (low), then P2 buzzer beeps (high)
 *   hold SURR     that side's buzzer sounds while held: P1 1 kHz, P2 2 kHz
 *                 (both held: the two tones take turns, they share one timer)
 *   MUTE on       that side goes silent, even while its SURR is held
 *   MUTE off      short chirp on that side
 *
 * Live Expressions:
 *   test_side_surr[s]        1 while surrender button s is held
 *   test_side_surr_count[s]  presses counted (one per press: more = bouncing)
 *   test_side_mute[s]        1 = mute switch on (switch leg at GND)
 *   test_side_hz[s]          tone buzzer s is playing now (0 = silent)
 */
#include "main.h"
#include "hwtests.h"
#include <stdbool.h>

/* ---------------- settings ---------------- */
#define TONE_P1_HZ     1000U
#define TONE_P2_HZ     2000U
#define CHIRP_HZ        1500U    /* "sound back on" chirp */
#define CHIRP_MS       150U
#define READ_MS        5U       /* switches read every 5 ms ... */
#define STABLE_READS   4U       /* ... and must read the same 4 times (20 ms) */

typedef struct { GPIO_TypeDef *port; uint16_t pin; } Pin;

static const Pin surr_pin[2] = { {GPIOE, GPIO_PIN_10}, {GPIOE, GPIO_PIN_12} };
static const Pin mute_pin[2] = { {GPIOE, GPIO_PIN_0},  {GPIOB, GPIO_PIN_1} };
static const uint16_t tone_hz[2] = { TONE_P1_HZ, TONE_P2_HZ };

volatile uint8_t  test_side_surr[2];
volatile uint16_t test_side_surr_count[2];
volatile uint8_t  test_side_mute[2];
volatile uint16_t test_side_hz[2];

static bool started;
static uint32_t start_ms, read_ms;
static uint8_t surr_reads[2], mute_reads[2];    /* reads in a row that differ from the state */
static uint32_t chirp_until[2];
static uint16_t played_arr;                    /* timer period now loaded (0 = none yet) */

/* ---------------- TIM3: CH1 = P1 buzzer, CH2 = P2 buzzer, 1 MHz count ---------------- */

static uint32_t tim3_clock_hz(void)
{
    /* APB1 timers run at 2 x PCLK1 unless the APB1 prescaler is 1 */
    RCC_ClkInitTypeDef clk;
    uint32_t latency;
    HAL_RCC_GetClockConfig(&clk, &latency);
    return (clk.APB1CLKDivider == RCC_HCLK_DIV1) ? HAL_RCC_GetPCLK1Freq() : 2U * HAL_RCC_GetPCLK1Freq();
}

/* Square wave of `hz` on the buzzers whose hz[] is non-zero (the two share
 * one timer, so they can only play the same tone at the same moment). */
static void play(const uint16_t hz[2])
{
    const uint16_t f = hz[0] != 0U ? hz[0] : hz[1];
    uint16_t arr = played_arr;
    if (f != 0U) {
        arr = (uint16_t)(1000000U / f - 1U);
        if (arr != played_arr) {
            TIM3->ARR = arr;
            played_arr = arr;
        }
    }
    TIM3->CCR1 = hz[0] != 0U ? (arr + 1U) / 2U : 0U;   /* 50 % duty, 0 = line low = silent */
    TIM3->CCR2 = hz[1] != 0U ? (arr + 1U) / 2U : 0U;
    test_side_hz[0] = hz[0];
    test_side_hz[1] = hz[1];
}

static void sound_init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    g.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOC, &g);

    TIM3->CR1 = 0;
    TIM3->PSC = tim3_clock_hz() / 1000000U - 1U;     /* count at 1 MHz */
    TIM3->ARR = 999;
    TIM3->CCMR1 = TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE   /* PWM mode 1 + preload */
                | TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2PE;
    TIM3->CCR1 = 0;
    TIM3->CCR2 = 0;
    TIM3->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

/* ---------------- switches ---------------- */

static bool is_low(Pin p) { return HAL_GPIO_ReadPin(p.port, p.pin) == GPIO_PIN_RESET; }

/* Debounce one switch: `state` changes after STABLE_READS reads that disagree with it. */
static bool settle(volatile uint8_t *state, uint8_t *reads, bool now_low)
{
    if (now_low == (*state != 0U)) {
        *reads = 0;
        return false;
    }
    if (++*reads < STABLE_READS) {
        return false;
    }
    *reads = 0;
    *state = now_low;
    return true;                        /* changed */
}

static void read_switches(uint32_t now)
{
    uint8_t s;
    for (s = 0; s < 2U; ++s) {
        if (settle(&test_side_surr[s], &surr_reads[s], is_low(surr_pin[s])) && test_side_surr[s]) {
            ++test_side_surr_count[s];
        }
        if (settle(&test_side_mute[s], &mute_reads[s], is_low(mute_pin[s])) && !test_side_mute[s]) {
            chirp_until[s] = now + CHIRP_MS;          /* sound back on: chirp */
        }
    }
}

/* ---------------- test ---------------- */

void TestSide_Init(void)
{
    GPIO_InitTypeDef g = {0};
    uint8_t s;
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;               /* switch closed = pin at GND = low */
    for (s = 0; s < 2U; ++s) {
        g.Pin = surr_pin[s].pin;
        HAL_GPIO_Init(surr_pin[s].port, &g);
        g.Pin = mute_pin[s].pin;
        HAL_GPIO_Init(mute_pin[s].port, &g);
    }
    sound_init();
}

void TestSide_Loop(void)
{
    const uint32_t now = HAL_GetTick();
    uint16_t hz[2] = { 0, 0 };
    uint8_t s;

    if (!started) {                     /* first call: both buzzers beep once */
        started = true;
        start_ms = now;
        for (s = 0; s < 2U; ++s) {      /* take the switches as they are now */
            test_side_mute[s] = is_low(mute_pin[s]);
            test_side_surr[s] = 0;
        }
    }
    if (now - read_ms >= READ_MS) {
        read_ms = now;
        read_switches(now);
    }

    if (now - start_ms < 200U) {
        hz[0] = TONE_P1_HZ;             /* start-up beep P1 */
    } else if (now - start_ms >= 350U && now - start_ms < 550U) {
        hz[1] = TONE_P2_HZ;             /* start-up beep P2 */
    }
    for (s = 0; s < 2U; ++s) {
        if (test_side_surr[s]) {
            hz[s] = tone_hz[s];
        } else if ((int32_t)(chirp_until[s] - now) > 0) {
            hz[s] = CHIRP_HZ;
        }
        if (test_side_mute[s]) {
            hz[s] = 0;
        }
    }
    if (hz[0] != 0U && hz[1] != 0U && hz[0] != hz[1]) {
        hz[((now / 100U) & 1U) ? 0 : 1] = 0;          /* different tones: take turns every 100 ms */
    }
    play(hz);
}
