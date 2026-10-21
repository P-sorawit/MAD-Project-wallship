/* TEST 1: WS2812 LED strips (TIM1 CH1-4 + DMA2).
 * Files: this + 2_led/ws2812.c, ws2812_port_stm32.c, ws2812_test.c
 * Every LED cycles: RED -> GREEN -> BLUE -> WHITE (1.5 s each)
 *                   -> channel colors (CH1 red, CH2 green, CH3 blue, CH4 yellow)
 *                   -> one LED walks A1, B1 ... H1, A2 ... H7 (150 ms each)
 * Watch: test_led_pattern (1..6), test_led_frames (DMA frames completed).
 */
#include "main.h"
#include "hwtests.h"
#include "ws2812.h"
#include "ws2812_test.h"

void Ws2812_Port_PulseFinished(TIM_HandleTypeDef *htim);

volatile uint8_t test_led_pattern;
volatile uint32_t test_led_frames;

void TestLed_Init(void)
{
    Ws2812Test_Init();
}

void TestLed_Loop(void)
{
    const bool was_busy = Ws2812_IsBusy();
    Ws2812Test_Loop(HAL_GetTick());
    test_led_pattern = Ws2812Test_Pattern();
    if (was_busy && !Ws2812_IsBusy()) {
        ++test_led_frames;      /* stays 0 => DMA-complete interrupt never fires */
    }
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    Ws2812_Port_PulseFinished(htim);
}
