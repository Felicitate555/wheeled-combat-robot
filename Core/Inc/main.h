/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define A2_Pin GPIO_PIN_3
#define A2_GPIO_Port GPIOE
#define A1_Pin GPIO_PIN_5
#define A1_GPIO_Port GPIOE
#define IR10_Pin GPIO_PIN_0
#define IR10_GPIO_Port GPIOF
#define IR9_Pin GPIO_PIN_2
#define IR9_GPIO_Port GPIOF
#define B2_Pin GPIO_PIN_7
#define B2_GPIO_Port GPIOF
#define IR8_Pin GPIO_PIN_8
#define IR8_GPIO_Port GPIOF
#define B1_Pin GPIO_PIN_9
#define B1_GPIO_Port GPIOF
#define PWMA_Pin GPIO_PIN_0
#define PWMA_GPIO_Port GPIOA
#define PWMB_Pin GPIO_PIN_2
#define PWMB_GPIO_Port GPIOA
#define Range5_Pin GPIO_PIN_3
#define Range5_GPIO_Port GPIOA
#define Range1_Pin GPIO_PIN_6
#define Range1_GPIO_Port GPIOA
#define Range2_Pin GPIO_PIN_7
#define Range2_GPIO_Port GPIOA
#define Range3_Pin GPIO_PIN_4
#define Range3_GPIO_Port GPIOC
#define Range4_Pin GPIO_PIN_5
#define Range4_GPIO_Port GPIOC
#define IR7_Pin GPIO_PIN_1
#define IR7_GPIO_Port GPIOB
#define IR6_Pin GPIO_PIN_11
#define IR6_GPIO_Port GPIOB
#define IR4_Pin GPIO_PIN_13
#define IR4_GPIO_Port GPIOB
#define IR5_Pin GPIO_PIN_14
#define IR5_GPIO_Port GPIOB
#define IR3_Pin GPIO_PIN_15
#define IR3_GPIO_Port GPIOB
#define IR2_Pin GPIO_PIN_10
#define IR2_GPIO_Port GPIOA
#define IR1_Pin GPIO_PIN_12
#define IR1_GPIO_Port GPIOA
#define LED_Pin GPIO_PIN_1
#define LED_GPIO_Port GPIOE
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
