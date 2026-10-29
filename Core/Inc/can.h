/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.h
  * @brief   This file contains all the function prototypes for
  *          the can.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CAN_H__
#define __CAN_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern CAN_HandleTypeDef hcan;

/* USER CODE BEGIN Private defines */
#define F072_CAN_RX_ID  2U   /* CAN StdId this node receives */
#define F072_CAN_DIAG_REQUEST_ID 0x600U /* Standard data frame, echo test */
#define F072_CAN_DIAG_REPLY_ID   0x601U /* Echoes request DLC and payload */
#define F072_CMD_RED    3U   /* data=3 -> show red R logo */
#define F072_CMD_BLUE   4U   /* data=4 -> show blue R logo */
#define F072_CMD_OFF    0U   /* Low-load test build: clear the complete panel */
/* USER CODE END Private defines */

void MX_CAN_Init(void);

/* USER CODE BEGIN Prototypes */
extern volatile uint8_t  g_led_cmd;
extern volatile uint32_t g_can_rx_count;   /* debug: +1 on each received CAN frame */
extern volatile uint32_t g_led_show_count; /* debug: +1 each time the LED show runs */
extern volatile uint32_t g_can_cmd_count;
extern volatile uint32_t g_can_tx_count;
extern volatile uint32_t g_can_error_count;
extern volatile uint32_t g_can_last_error;
extern volatile uint32_t g_can_last_error_esr;
extern volatile uint32_t g_can_reply_drop_count;
void CAN_Process(void);

uint32_t CAN_Loopback_Test(void);          /* DEBUG: 回环自测，发一帧并等回环 */
extern volatile uint8_t  g_lb_received;    /* DEBUG: 1 = 测试帧被回环收到 */
extern volatile uint32_t g_lb_rx_count;    /* DEBUG: 累计收到的回环帧数 */
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __CAN_H__ */

