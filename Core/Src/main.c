/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ws2812_image_red.h"
#include "ws2812_image_blue.h"
#include "ws2812_pwm.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
#if defined(LED_LOW_LOAD_TEST)
volatile uint32_t g_led_low_load_mode;
volatile uint32_t g_led_test_lit_count = 0U;
#endif

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CAN_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  WS2812_Init();
#if defined(LED_LOW_LOAD_TEST)
  /* Clear retained panel colours before any one/three-pixel load test. */
  g_led_low_load_mode = 1U;
  if (WS2812_Clear(WS2812_LED_COUNT) != HAL_OK)
  {
    Error_Handler();
  }
#endif

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

#if 0
    /* ============ DEBUG: CAN 回环自测（每500ms发一帧，红灯=通过 蓝灯=失败） ============ */
    {
      static uint32_t lb_tick    = 0U;   /* 上次发送时刻 */
      static uint8_t  lb_pending = 0U;   /* 1=已发一帧，等下一个周期看结果 */

      if ((HAL_GetTick() - lb_tick) >= 500U)
      {
        lb_tick = HAL_GetTick();

        if (lb_pending != 0U)
        {
          /* 上一帧的回环结果已经出来 */
          while (WS2812_IsBusy() != 0U)
          {
          }

          if (g_lb_received != 0U)
          {
            (void)WS2812_Show(ws2812_image_red, WS2812_LED_COUNT);   /* 收到回环 -> 红灯 */
          }
          else
          {
            (void)WS2812_Show(ws2812_image_blue, WS2812_LED_COUNT);  /* 没收到   -> 蓝灯 */
          }
        }

        (void)CAN_Loopback_Test();   /* 发一帧测试帧 */
        lb_pending = 1U;
      }
    }
    /* ============ DEBUG END ============ */
#endif

//    /* ============ 1s 自动测试：红/蓝交替亮灯（不需要时注释掉整块即可） ============ */
//    {
//      static uint32_t test_tick = 0U;   /* 上次切换的时刻 */
//      static uint8_t  test_red  = 1U;   /* 1=下次亮红, 0=下次亮蓝 */

//      if ((HAL_GetTick() - test_tick) >= 1000U)
//      {
//        test_tick = HAL_GetTick();

//        while (WS2812_IsBusy() != 0U)   /* 等上一次 DMA 传完 */
//        {
//        }

//        if (test_red != 0U)
//        {
//          (void)WS2812_Show(ws2812_image_red, WS2812_LED_COUNT);
//        }
//        else
//        {
//          (void)WS2812_Show(ws2812_image_blue, WS2812_LED_COUNT);
//        }

//        test_red = (test_red != 0U) ? 0U : 1U;   /* 翻转颜色 */
//      }
//    }
//    /* ============ TEST END ============ */

    /* ============ TEST: alternate red/blue blink every 1s — DELETE AFTER TEST ============ */

    CAN_Process();

    /* Keep servicing CAN while the previous LED DMA transfer completes. */
    {
      static uint8_t last_cmd = 0U;
      const uint8_t cmd = g_led_cmd;
      if ((cmd != last_cmd) && (WS2812_IsBusy() == 0U))
      {
#if defined(LED_LOW_LOAD_TEST)
        uint16_t lit_count = 0U;
        ws2812_rgb_t colour = {0U, 0U, 0U};
        if (cmd == F072_CMD_RED) { lit_count = 1U; colour.r = 16U; }
        else if (cmd == F072_CMD_BLUE) { lit_count = 3U; colour.b = 16U; }
        if (WS2812_ShowPrefix(lit_count, colour) == HAL_OK)
        {
          g_led_test_lit_count = lit_count;
          last_cmd = cmd;
          g_led_show_count++;
        }
#else
        const ws2812_rgb_t *image = NULL;
        if (cmd == F072_CMD_RED) { image = ws2812_image_red; }
        else if (cmd == F072_CMD_BLUE) { image = ws2812_image_blue; }
        if ((image != NULL) &&
            (WS2812_Show(image, WS2812_LED_COUNT) == HAL_OK))
        {
          last_cmd = cmd;
          g_led_show_count++;
        }
#endif
      }
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
