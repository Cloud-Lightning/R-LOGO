/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.c
  * @brief   This file provides code for the configuration
  *          of the CAN instances.
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
#include "can.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

CAN_HandleTypeDef hcan;

/* CAN init function */
void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN;
  hcan.Init.Prescaler = 3;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_4TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_11TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_4TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = ENABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = ENABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  /* Full-pass filter (mask=0 accepts every ID); StdId matching is done in the RX callback. */
  CAN_FilterTypeDef sFilterConfig = {0};
  sFilterConfig.FilterIdHigh = 0U;
  sFilterConfig.FilterIdLow = 0U;
  sFilterConfig.FilterMaskIdHigh = 0U;
  sFilterConfig.FilterMaskIdLow = 0U;
  sFilterConfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  sFilterConfig.FilterBank = 0U;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterActivation = CAN_FILTER_ENABLE;
  sFilterConfig.SlaveStartFilterBank = 0U;
  if (HAL_CAN_ConfigFilter(&hcan, &sFilterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /* Enable FIFO0 message-pending interrupt and start the CAN peripheral. */
  if (HAL_CAN_ActivateNotification(&hcan,
      CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO0_OVERRUN |
      CAN_IT_TX_MAILBOX_EMPTY | CAN_IT_ERROR_WARNING | CAN_IT_ERROR_PASSIVE |
      CAN_IT_BUSOFF | CAN_IT_LAST_ERROR_CODE | CAN_IT_ERROR) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_NVIC_SetPriority(CEC_CAN_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(CEC_CAN_IRQn);
  if (HAL_CAN_Start(&hcan) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE END CAN_Init 2 */

}

void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(canHandle->Instance==CAN)
  {
  /* USER CODE BEGIN CAN_MspInit 0 */

  /* USER CODE END CAN_MspInit 0 */
    /* CAN clock enable */
    __HAL_RCC_CAN1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**CAN GPIO Configuration
    PA11     ------> CAN_RX
    PA12     ------> CAN_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_CAN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN CAN_MspInit 1 */

  /* USER CODE END CAN_MspInit 1 */
  }
}

void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{

  if(canHandle->Instance==CAN)
  {
  /* USER CODE BEGIN CAN_MspDeInit 0 */

  /* USER CODE END CAN_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CAN1_CLK_DISABLE();

    /**CAN GPIO Configuration
    PA11     ------> CAN_RX
    PA12     ------> CAN_TX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11|GPIO_PIN_12);

  /* USER CODE BEGIN CAN_MspDeInit 1 */

  /* USER CODE END CAN_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

volatile uint8_t  g_led_cmd = 0U;
volatile uint32_t g_can_rx_count = 0U;   /* debug: +1 on each received CAN frame */
volatile uint32_t g_led_show_count = 0U; /* debug: +1 each time the LED show runs */

volatile uint32_t g_can_cmd_count = 0U;
volatile uint32_t g_can_tx_count = 0U;
volatile uint32_t g_can_error_count = 0U;
volatile uint32_t g_can_last_error = 0U;
volatile uint32_t g_can_last_error_esr = 0U;
volatile uint32_t g_can_reply_drop_count = 0U;

/* A separate diagnostic ID leaves the existing LED command protocol intact.
 * ISR produces replies, main consumes them. Reserve one slot to detect full. */
#define CAN_REPLY_QUEUE_SIZE 8U
typedef struct
{
  uint8_t dlc;
  uint8_t data[8];
} CAN_Reply;
static CAN_Reply s_replies[CAN_REPLY_QUEUE_SIZE];
static volatile uint8_t s_reply_write;
static volatile uint8_t s_reply_read;

void CAN_Process(void)
{
  while ((s_reply_read != s_reply_write) &&
         (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) != 0U))
  {
    CAN_TxHeaderTypeDef header = {0};
    uint32_t mailbox;
    CAN_Reply *reply = &s_replies[s_reply_read];
    header.StdId = F072_CAN_DIAG_REPLY_ID;
    header.IDE = CAN_ID_STD;
    header.RTR = CAN_RTR_DATA;
    header.DLC = reply->dlc;
    if (HAL_CAN_AddTxMessage(&hcan, &header, reply->data, &mailbox) != HAL_OK)
    {
      break;
    }
    s_reply_read = (uint8_t)((s_reply_read + 1U) % CAN_REPLY_QUEUE_SIZE);
  }
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *handle)
{
  if (handle->Instance == CAN) { g_can_tx_count++; }
}

void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *handle)
{
  if (handle->Instance == CAN) { g_can_tx_count++; }
}

void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *handle)
{
  if (handle->Instance == CAN) { g_can_tx_count++; }
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *handle)
{
  g_can_error_count++;
  g_can_last_error = HAL_CAN_GetError(handle);
  g_can_last_error_esr = handle->Instance->ESR;
  (void)HAL_CAN_ResetError(handle);
}

/* ============ DEBUG: CAN 回环自测 ============ */
#define CAN_LB_TEST_ID 0x200U   /* 测试帧的 StdId */

volatile uint8_t  g_lb_received = 0U;   /* 1 = 测试帧被回环收到 */
volatile uint32_t g_lb_rx_count = 0U;   /* 累计收到的回环帧数 */

/* 发一帧测试帧（回环模式下芯片内部会把它回送到自己的 RX）。 */
uint32_t CAN_Loopback_Test(void)
{
  CAN_TxHeaderTypeDef txHeader;
  uint8_t txData[8] = {0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U, 0x88U};
  uint32_t txMailbox = 0U;

  g_lb_received = 0U;   /* 先清标志，再发送 */

  txHeader.StdId              = CAN_LB_TEST_ID;
  txHeader.ExtId              = 0U;
  txHeader.IDE                = CAN_ID_STD;
  txHeader.RTR                = CAN_RTR_DATA;
  txHeader.DLC                = 8U;
  txHeader.TransmitGlobalTime = DISABLE;

  return HAL_CAN_AddTxMessage(&hcan, &txHeader, txData, &txMailbox);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  CAN_RxHeaderTypeDef rxHeader;
  uint8_t rxData[8] = {0U};

  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, rxData) != HAL_OK)
  {
    return;
  }

  g_can_rx_count++;   /* debug: CAN frame successfully received */

  if ((rxHeader.IDE == CAN_ID_STD) && (rxHeader.RTR == CAN_RTR_DATA) &&
      (rxHeader.DLC >= 1U) && (rxHeader.DLC <= 8U) &&
      (rxHeader.StdId == F072_CAN_RX_ID) &&
      ((rxData[0] == F072_CMD_RED) || (rxData[0] == F072_CMD_BLUE)
#if defined(LED_LOW_LOAD_TEST)
       || (rxData[0] == F072_CMD_OFF)
#endif
      ))
  {
    g_led_cmd = rxData[0];
    g_can_cmd_count++;
  }

  if ((rxHeader.IDE == CAN_ID_STD) && (rxHeader.RTR == CAN_RTR_DATA) &&
      (rxHeader.DLC <= 8U) && (rxHeader.StdId == F072_CAN_DIAG_REQUEST_ID))
  {
    uint8_t next = (uint8_t)((s_reply_write + 1U) % CAN_REPLY_QUEUE_SIZE);
    if (next == s_reply_read)
    {
      g_can_reply_drop_count++;
    }
    else
    {
      CAN_Reply *reply = &s_replies[s_reply_write];
      reply->dlc = (uint8_t)rxHeader.DLC;
      for (uint32_t i = 0U; i < rxHeader.DLC; i++)
      {
        reply->data[i] = rxData[i];
      }
      __DMB();
      s_reply_write = next;
    }
  }

  if (rxHeader.StdId == CAN_LB_TEST_ID)   /* 回环测试帧回来了 */
  {
    g_lb_rx_count++;
    g_lb_received = 1U;
  }
}

/* USER CODE END 1 */
