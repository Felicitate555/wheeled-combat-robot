/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for
  *          the adc.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern DMA_HandleTypeDef hdma_adc1;

/* USER CODE BEGIN Private defines */

#define ADC1_DMA_ACTIVE_WORDS       4U
#define ADC1_DMA_STORAGE_WORDS      8U
#define ADC1_DMA_BUFFER_ADDRESS     0x2404FFE0UL
#define ADC1_DMA_RECOVERY_RETRY_MS  10U

typedef struct
{
  uint32_t range[ADC1_DMA_ACTIVE_WORDS];
  uint32_t frame_count;
  uint32_t error_count;
  uint32_t last_update_ms;
  uint8_t valid;
} ADC1_DMA_Frame;

/* USER CODE END Private defines */

void MX_ADC1_Init(void);
void MX_ADC2_Init(void);

/* USER CODE BEGIN Prototypes */

HAL_StatusTypeDef ADC1_DMA_Start(void);
void ADC1_DMA_Service(void);
uint8_t ADC1_DMA_ReadLatest(ADC1_DMA_Frame *frame);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
