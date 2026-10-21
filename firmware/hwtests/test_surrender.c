/* TEST 4: surrender push buttons (EXTI falling edge).
 * Files: this only.
 * Uses the same filter as the game (app.c): 300 ms debounce + "still held".
 *
 * Watch:
 *   test_surr_level[2]   live pin level: 1 = released, 0 = pressed
 *   test_surr_edges[2]   every EXTI interrupt, contact bounce included
 *   test_surr_presses[2] presses after the filter -> must be +1 per press
 *   test_surr_bounce_max how many extra edges the worst press produced
 */
#include "main.h"
#include "hwtests.h"

#define DEBOUNCE_MS 300U

volatile uint8_t test_surr_level[2];
volatile uint16_t test_surr_edges[2];
volatile uint16_t test_surr_presses[2];
volatile uint16_t test_surr_bounce_max;

static uint32_t last_ms[2];
static uint16_t edges_at_press[2];

void TestSurrender_Init(void)
{
    last_ms[0] = last_ms[1] = HAL_GetTick() - DEBOUNCE_MS;
}

void TestSurrender_Loop(void)
{
    uint8_t p;
    test_surr_level[0] = (uint8_t)HAL_GPIO_ReadPin(SURR1_GPIO_Port, SURR1_Pin);
    test_surr_level[1] = (uint8_t)HAL_GPIO_ReadPin(SURR2_GPIO_Port, SURR2_Pin);
    for (p = 0; p < 2U; ++p) {
        /* bounce measured once the button has been released for 300 ms */
        if (test_surr_level[p] && HAL_GetTick() - last_ms[p] > DEBOUNCE_MS &&
            test_surr_edges[p] > edges_at_press[p] + 1U) {
            const uint16_t extra = (uint16_t)(test_surr_edges[p] - edges_at_press[p] - 1U);
            if (extra > test_surr_bounce_max) {
                test_surr_bounce_max = extra;
            }
            edges_at_press[p] = test_surr_edges[p];
        }
    }
}

static void on_edge(uint8_t p, GPIO_TypeDef *port, uint16_t pin)
{
    const uint32_t now = HAL_GetTick();
    ++test_surr_edges[p];
    if (now - last_ms[p] < DEBOUNCE_MS || HAL_GPIO_ReadPin(port, pin) != GPIO_PIN_RESET) {
        return;
    }
    last_ms[p] = now;
    edges_at_press[p] = (uint16_t)(test_surr_edges[p] - 1U);
    ++test_surr_presses[p];
}

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin == SURR1_Pin) {
        on_edge(0, SURR1_GPIO_Port, SURR1_Pin);
    } else if (pin == SURR2_Pin) {
        on_edge(1, SURR2_GPIO_Port, SURR2_Pin);
    }
}
