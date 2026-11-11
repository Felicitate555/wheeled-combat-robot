/** 
  ******************************************************************************
  * @file    adc.c
  * @brief   This file provides code for the configuration
  *          of the ADC instances.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "adc.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
DMA_HandleTypeDef hdma_adc1;

typedef struct
{
  uint32_t word[ADC1_DMA_STORAGE_WORDS];
} ADC1_DMA_Storage;

/* 链接脚本将该对象固定在 AXI-SRAM 中独占的缓存行上。 */
__attribute__((section("ADC_DMA_BUFFER"), aligned(32)))
volatile ADC1_DMA_Storage g_adc1_dma_buffer;

volatile uint32_t g_adc1_dma_frame_count = 0U;
volatile uint32_t g_adc1_dma_error_count = 0U;
volatile uint32_t g_adc1_dma_last_update_ms = 0U;
volatile uint8_t g_adc1_dma_valid = 0U;

static volatile uint32_t g_adc1_dma_publish_sequence = 0U;
static volatile uint32_t g_adc1_dma_published[ADC1_DMA_ACTIVE_WORDS] = {0U};
static volatile uint8_t g_adc1_dma_restart_requested = 0U;
static uint32_t g_adc1_dma_last_restart_ms = 0U;

typedef char ADC1_DMA_Storage_Must_Be_32_Bytes
  [(sizeof(ADC1_DMA_Storage) == 32U) ? 1 : -1];

/* ADC1 init function */
void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */
  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_16B;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 4;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.NbrOfDiscConversion = 1;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T3_TRGO;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc1.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_64CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  sConfig.OffsetSignedSaturation = DISABLE;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_7;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_8;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}
/* ADC2 init function */
void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */
  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
  hadc2.Init.Resolution = ADC_RESOLUTION_16B;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DiscontinuousConvMode = ENABLE;
  hadc2.Init.NbrOfDiscConversion = 1;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc2.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc2.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc2.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_15;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  sConfig.OffsetSignedSaturation = DISABLE;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

static uint32_t HAL_RCC_ADC12_CLK_ENABLED=0;

void HAL_ADC_MspInit(ADC_HandleTypeDef* adcHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(adcHandle->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspInit 0 */

  /* USER CODE END ADC1_MspInit 0 */
    /* ADC1 clock enable */
    HAL_RCC_ADC12_CLK_ENABLED++;
    if(HAL_RCC_ADC12_CLK_ENABLED==1){
      __HAL_RCC_ADC12_CLK_ENABLE();
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**ADC1 GPIO Configuration
    PA6     ------> ADC1_INP3
    PA7     ------> ADC1_INP7
    PC4     ------> ADC1_INP4
    PC5     ------> ADC1_INP8
    */
    GPIO_InitStruct.Pin = Range1_Pin|Range2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = Range3_Pin|Range4_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* ADC1 DMA Init */
    hdma_adc1.Instance = DMA1_Stream0;
    hdma_adc1.Init.Request = DMA_REQUEST_ADC1;
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma_adc1.Init.Mode = DMA_CIRCULAR;
    hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_adc1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(adcHandle, DMA_Handle, hdma_adc1);

    HAL_NVIC_SetPriority(ADC_IRQn, 0, 1);
    HAL_NVIC_EnableIRQ(ADC_IRQn);

  /* USER CODE BEGIN ADC1_MspInit 1 */

  /* USER CODE END ADC1_MspInit 1 */
  }
  else if(adcHandle->Instance==ADC2)
  {
  /* USER CODE BEGIN ADC2_MspInit 0 */

  /* USER CODE END ADC2_MspInit 0 */
    /* ADC2 clock enable */
    HAL_RCC_ADC12_CLK_ENABLED++;
    if(HAL_RCC_ADC12_CLK_ENABLED==1){
      __HAL_RCC_ADC12_CLK_ENABLE();
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**ADC2 GPIO Configuration
    PA3     ------> ADC2_INP15
    */
    GPIO_InitStruct.Pin = Range5_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(Range5_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN ADC2_MspInit 1 */

  /* USER CODE END ADC2_MspInit 1 */
  }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef* adcHandle)
{

  if(adcHandle->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspDeInit 0 */

  /* USER CODE END ADC1_MspDeInit 0 */
    /* Peripheral clock disable */
    HAL_RCC_ADC12_CLK_ENABLED--;
    if(HAL_RCC_ADC12_CLK_ENABLED==0){
      __HAL_RCC_ADC12_CLK_DISABLE();
    }

    /**ADC1 GPIO Configuration
    PA6     ------> ADC1_INP3
    PA7     ------> ADC1_INP7
    PC4     ------> ADC1_INP4
    PC5     ------> ADC1_INP8
    */
    HAL_GPIO_DeInit(GPIOA, Range1_Pin|Range2_Pin);

    HAL_GPIO_DeInit(GPIOC, Range3_Pin|Range4_Pin);

    HAL_DMA_DeInit(adcHandle->DMA_Handle);
    HAL_NVIC_DisableIRQ(ADC_IRQn);

  /* USER CODE BEGIN ADC1_MspDeInit 1 */

  /* USER CODE END ADC1_MspDeInit 1 */
  }
  else if(adcHandle->Instance==ADC2)
  {
  /* USER CODE BEGIN ADC2_MspDeInit 0 */

  /* USER CODE END ADC2_MspDeInit 0 */
    /* Peripheral clock disable */
    HAL_RCC_ADC12_CLK_ENABLED--;
    if(HAL_RCC_ADC12_CLK_ENABLED==0){
      __HAL_RCC_ADC12_CLK_DISABLE();
    }

    /**ADC2 GPIO Configuration
    PA3     ------> ADC2_INP15
    */
    HAL_GPIO_DeInit(Range5_GPIO_Port, Range5_Pin);

  /* USER CODE BEGIN ADC2_MspDeInit 1 */

  /* USER CODE END ADC2_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

HAL_StatusTypeDef ADC1_DMA_Start(void)
{
  uint32_t index;
  HAL_StatusTypeDef status;
  uintptr_t buffer_address = (uintptr_t)&g_adc1_dma_buffer.word[0];

  g_adc1_dma_valid = 0U;
  g_adc1_dma_frame_count = 0U;
  g_adc1_dma_error_count = 0U;
  g_adc1_dma_last_update_ms = 0U;
  g_adc1_dma_publish_sequence = 0U;
  g_adc1_dma_restart_requested = 0U;
  g_adc1_dma_last_restart_ms = HAL_GetTick();

  for (index = 0U; index < ADC1_DMA_STORAGE_WORDS; index++)
  {
    g_adc1_dma_buffer.word[index] = 0U;
  }
  for (index = 0U; index < ADC1_DMA_ACTIVE_WORDS; index++)
  {
    g_adc1_dma_published[index] = 0U;
  }

  /* 本工程有意保持 D-Cache 关闭。如果后续平台改动破坏该约定，
     应直接显式报错，避免使用过期的 ADC 数据。 */
  if (((SCB->CCR & SCB_CCR_DC_Msk) != 0U) ||
      (buffer_address != ADC1_DMA_BUFFER_ADDRESS) ||
      ((buffer_address & 31U) != 0U))
  {
    g_adc1_dma_error_count++;
    return HAL_ERROR;
  }

  status = HAL_ADCEx_Calibration_Start(&hadc1,
                                       ADC_CALIB_OFFSET,
                                       ADC_SINGLE_ENDED);
  if (status != HAL_OK)
  {
    g_adc1_dma_error_count++;
    return status;
  }

  status = HAL_ADC_Start_DMA(&hadc1,
                            (uint32_t *)(uintptr_t)buffer_address,
                            ADC1_DMA_ACTIVE_WORDS);
  if (status != HAL_OK)
  {
    g_adc1_dma_error_count++;
    return status;
  }

  /* 仅在四个转换序位全部传输完成后发布一帧数据。 */
  __HAL_DMA_DISABLE_IT(&hdma_adc1, DMA_IT_HT);
  return HAL_OK;
}

void ADC1_DMA_Service(void)
{
  uint32_t now_ms;
  HAL_StatusTypeDef status;

  if (g_adc1_dma_restart_requested == 0U)
  {
    return;
  }

  now_ms = HAL_GetTick();
  if ((uint32_t)(now_ms - g_adc1_dma_last_restart_ms) <
      ADC1_DMA_RECOVERY_RETRY_MS)
  {
    return;
  }
  g_adc1_dma_last_restart_ms = now_ms;
  g_adc1_dma_valid = 0U;

  status = HAL_ADC_Stop_DMA(&hadc1);
  if (status != HAL_OK)
  {
    g_adc1_dma_error_count++;
    return;
  }

  status = HAL_ADC_Start_DMA(&hadc1,
                             (uint32_t *)(uintptr_t)&g_adc1_dma_buffer.word[0],
                             ADC1_DMA_ACTIVE_WORDS);
  if (status == HAL_OK)
  {
    __HAL_DMA_DISABLE_IT(&hdma_adc1, DMA_IT_HT);
    g_adc1_dma_restart_requested = 0U;
  }
  else
  {
    g_adc1_dma_error_count++;
  }
}

uint8_t ADC1_DMA_ReadLatest(ADC1_DMA_Frame *frame)
{
  uint32_t sequence_before;
  uint32_t sequence_after;
  uint32_t index;

  if (frame == NULL)
  {
    return 0U;
  }

  do
  {
    sequence_before = g_adc1_dma_publish_sequence;
    sequence_after = sequence_before;
    if ((sequence_before & 1U) == 0U)
    {
      __DMB();
      for (index = 0U; index < ADC1_DMA_ACTIVE_WORDS; index++)
      {
        frame->range[index] = g_adc1_dma_published[index];
      }
      frame->frame_count = g_adc1_dma_frame_count;
      frame->error_count = g_adc1_dma_error_count;
      frame->last_update_ms = g_adc1_dma_last_update_ms;
      frame->valid = g_adc1_dma_valid;
      __DMB();

      sequence_after = g_adc1_dma_publish_sequence;
    }
  } while ((sequence_before != sequence_after) ||
           ((sequence_after & 1U) != 0U));

  return frame->valid;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  uint32_t index;

  if ((hadc == NULL) || (hadc->Instance != ADC1))
  {
    return;
  }

  g_adc1_dma_publish_sequence++;
  __DMB();
  for (index = 0U; index < ADC1_DMA_ACTIVE_WORDS; index++)
  {
    g_adc1_dma_published[index] = g_adc1_dma_buffer.word[index];
  }
  g_adc1_dma_frame_count++;
  g_adc1_dma_last_update_ms = HAL_GetTick();
  g_adc1_dma_valid = 1U;
  __DMB();
  g_adc1_dma_publish_sequence++;
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
  if ((hadc == NULL) || (hadc->Instance != ADC1))
  {
    return;
  }

  g_adc1_dma_publish_sequence++;
  __DMB();
  g_adc1_dma_error_count++;
  g_adc1_dma_valid = 0U;
  g_adc1_dma_restart_requested = 1U;
  __DMB();
  g_adc1_dma_publish_sequence++;
}

/* USER CODE END 1 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
