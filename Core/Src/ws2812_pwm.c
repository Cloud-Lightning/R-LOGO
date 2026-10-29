#include "ws2812_pwm.h"

#include "main.h"
#include "tim.h"

#define WS2812_BITS_PER_LED 24U
#define WS2812_RESET_SLOTS 80U
#define WS2812_BUFFER_WORDS ((WS2812_LED_COUNT * WS2812_BITS_PER_LED) + WS2812_RESET_SLOTS)

#define WS2812_PWM_PERIOD 59U
#define WS2812_PWM_ZERO 17U
#define WS2812_PWM_ONE 34U

static DMA_HandleTypeDef s_tim1_ch1_dma;
static uint16_t s_pwm_buffer[WS2812_BUFFER_WORDS];
static volatile uint8_t s_busy;
volatile uint32_t g_ws2812_complete_count;

static void WS2812_AppendByte(uint8_t value, uint16_t *index)
{
  for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U)
  {
    s_pwm_buffer[*index] = ((value & mask) != 0U) ? WS2812_PWM_ONE : WS2812_PWM_ZERO;
    (*index)++;
  }
}

static uint16_t WS2812_EncodePixels(const ws2812_rgb_t *pixels, uint16_t count)
{
  uint16_t index = 0U;

  for (uint16_t pixel = 0U; pixel < count; ++pixel)
  {
    WS2812_AppendByte(pixels[pixel].g, &index);
    WS2812_AppendByte(pixels[pixel].r, &index);
    WS2812_AppendByte(pixels[pixel].b, &index);
  }

  for (uint16_t reset = 0U; reset < WS2812_RESET_SLOTS; ++reset)
  {
    s_pwm_buffer[index++] = 0U;
  }

  return index;
}

static uint16_t WS2812_EncodeClear(uint16_t count)
{
  uint16_t index = 0U;

  for (uint16_t pixel = 0U; pixel < count; ++pixel)
  {
    for (uint8_t bit = 0U; bit < WS2812_BITS_PER_LED; ++bit)
    {
      s_pwm_buffer[index++] = WS2812_PWM_ZERO;
    }
  }

  for (uint16_t reset = 0U; reset < WS2812_RESET_SLOTS; ++reset)
  {
    s_pwm_buffer[index++] = 0U;
  }

  return index;
}

static HAL_StatusTypeDef WS2812_StartTransfer(uint16_t length)
{
  if (s_busy != 0U)
  {
    return HAL_BUSY;
  }

  s_busy = 1U;
  __HAL_TIM_SET_COUNTER(&htim1, 0U);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0U);

  HAL_StatusTypeDef status = HAL_TIM_PWM_Start_DMA(&htim1,
                                                   TIM_CHANNEL_1,
                                                   (const uint32_t *)s_pwm_buffer,
                                                   length);
  if (status != HAL_OK)
  {
    s_busy = 0U;
  }

  return status;
}

void WS2812_Init(void)
{
  __HAL_RCC_DMA1_CLK_ENABLE();

  s_tim1_ch1_dma.Instance = DMA1_Channel2;
  s_tim1_ch1_dma.Init.Direction = DMA_MEMORY_TO_PERIPH;
  s_tim1_ch1_dma.Init.PeriphInc = DMA_PINC_DISABLE;
  s_tim1_ch1_dma.Init.MemInc = DMA_MINC_ENABLE;
  s_tim1_ch1_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  s_tim1_ch1_dma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  s_tim1_ch1_dma.Init.Mode = DMA_NORMAL;
  s_tim1_ch1_dma.Init.Priority = DMA_PRIORITY_HIGH;

  (void)HAL_DMA_DeInit(&s_tim1_ch1_dma);
  if (HAL_DMA_Init(&s_tim1_ch1_dma) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_LINKDMA(&htim1, hdma[TIM_DMA_ID_CC1], s_tim1_ch1_dma);

  HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0U, 0U);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);

  __HAL_TIM_SET_AUTORELOAD(&htim1, WS2812_PWM_PERIOD);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0U);
  s_busy = 0U;
}

HAL_StatusTypeDef WS2812_Show(const ws2812_rgb_t *pixels, uint16_t count)
{
  if (((pixels == NULL) && (count != 0U)) || (count > WS2812_LED_COUNT))
  {
    return HAL_ERROR;
  }

  const uint16_t length = WS2812_EncodePixels(pixels, count);
  return WS2812_StartTransfer(length);
}

HAL_StatusTypeDef WS2812_Clear(uint16_t count)
{
  if (count > WS2812_LED_COUNT)
  {
    return HAL_ERROR;
  }

  const uint16_t length = WS2812_EncodeClear(count);
  return WS2812_StartTransfer(length);
}

HAL_StatusTypeDef WS2812_ShowPrefix(uint16_t lit_count, ws2812_rgb_t colour)
{
  uint16_t index = 0U;
  if (lit_count > WS2812_LED_COUNT)
  {
    return HAL_ERROR;
  }
  if (s_busy != 0U)
  {
    return HAL_BUSY;
  }
  /* Always clock the entire panel so old colours cannot remain downstream. */
  for (uint16_t pixel = 0U; pixel < WS2812_LED_COUNT; pixel++)
  {
    WS2812_AppendByte((pixel < lit_count) ? colour.g : 0U, &index);
    WS2812_AppendByte((pixel < lit_count) ? colour.r : 0U, &index);
    WS2812_AppendByte((pixel < lit_count) ? colour.b : 0U, &index);
  }
  for (uint16_t reset = 0U; reset < WS2812_RESET_SLOTS; reset++)
  {
    s_pwm_buffer[index++] = 0U;
  }
  return WS2812_StartTransfer(index);
}

uint8_t WS2812_IsBusy(void)
{
  return s_busy;
}

void WS2812_DMA_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&s_tim1_ch1_dma);
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
  if ((htim != NULL) && (htim->Instance == TIM1))
  {
    (void)HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, 0U);
    s_busy = 0U;
    g_ws2812_complete_count++;
  }
}
