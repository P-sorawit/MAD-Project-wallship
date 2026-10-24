/* STM32 side of module 2. CubeMX setup expected:
 *   TIM1: clock 216 MHz, PSC = 0, ARR = 269, CH1-CH4 PWM mode 1
 *   DMA2: one stream per TIM1_CH1..CH4, Memory->Peripheral, Normal mode,
 *         memory Half Word, peripheral Half Word, memory increment on.
 */
#include "main.h"
#include "ws2812.h"

extern TIM_HandleTypeDef htim1;

static const uint32_t tim_channel[WS2812_CHANNELS] = {
    TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4
};
static const uint16_t dma_id[WS2812_CHANNELS] = {
    TIM_DMA_ID_CC1, TIM_DMA_ID_CC2, TIM_DMA_ID_CC3, TIM_DMA_ID_CC4
};

bool Ws2812_Port_Start(uint8_t channel, const uint16_t *buf, uint32_t len)
{
    if (htim1.hdma[dma_id[channel]] == NULL) {
        return false;   /* no DMA stream for this channel in CubeMX: skip, HAL would crash */
    }
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    /* Cortex-M7: if D-Cache is on, DMA reads RAM, not the cache. Flush first. */
    if (SCB->CCR & SCB_CCR_DC_Msk) {
        SCB_CleanDCache_by_Addr((uint32_t *)((uintptr_t)buf & ~(uintptr_t)31U),
                                (int32_t)(len * sizeof *buf + 32U));
    }
#endif
    return HAL_TIM_PWM_Start_DMA(&htim1, tim_channel[channel],
                                 (uint32_t *)(uintptr_t)buf, /* DMA is set to half-word */
                                 (uint16_t)len) == HAL_OK;
}

/* Call this from HAL_TIM_PWM_PulseFinishedCallback (see app.c). */
void Ws2812_Port_PulseFinished(TIM_HandleTypeDef *htim)
{
    uint8_t ch;
    if (htim->Instance != TIM1) {
        return;
    }
    for (ch = 0; ch < WS2812_CHANNELS; ++ch) {
        static const HAL_TIM_ActiveChannel active[WS2812_CHANNELS] = {
            HAL_TIM_ACTIVE_CHANNEL_1, HAL_TIM_ACTIVE_CHANNEL_2,
            HAL_TIM_ACTIVE_CHANNEL_3, HAL_TIM_ACTIVE_CHANNEL_4
        };
        if (htim->Channel == active[ch]) {
            HAL_TIM_PWM_Stop_DMA(&htim1, tim_channel[ch]);
            Ws2812_OnChannelDone(ch);
        }
    }
}
