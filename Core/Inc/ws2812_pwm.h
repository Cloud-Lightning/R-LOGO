#ifndef WS2812_PWM_H
#define WS2812_PWM_H

#include <stdint.h>

#include "stm32f0xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WS2812_LED_COUNT 256U

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} ws2812_rgb_t;

void WS2812_Init(void);
HAL_StatusTypeDef WS2812_Show(const ws2812_rgb_t *pixels, uint16_t count);
HAL_StatusTypeDef WS2812_Clear(uint16_t count);
/* Sends all 256 pixels; only the leading lit_count pixels have this colour. */
HAL_StatusTypeDef WS2812_ShowPrefix(uint16_t lit_count, ws2812_rgb_t colour);
extern volatile uint32_t g_ws2812_complete_count;
uint8_t WS2812_IsBusy(void);
void WS2812_DMA_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* WS2812_PWM_H */
