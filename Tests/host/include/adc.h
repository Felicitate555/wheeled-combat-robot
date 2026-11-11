#ifndef HOST_ADC_H__
#define HOST_ADC_H__

#include "main.h"

typedef struct { uint32_t dummy; } ADC_HandleTypeDef;
typedef struct { uint32_t dummy; } DMA_HandleTypeDef;

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern DMA_HandleTypeDef hdma_adc1;

#define ADC1_DMA_ACTIVE_WORDS 4U

typedef struct
{
  uint32_t range[ADC1_DMA_ACTIVE_WORDS];
  uint32_t frame_count;
  uint32_t error_count;
  uint32_t last_update_ms;
  uint8_t valid;
} ADC1_DMA_Frame;

#define ADC_FLAG_EOC        (1U << 0)
#define ADC_FLAG_OVR        (1U << 1)
#define ADC_CALIB_OFFSET    0U
#define ADC_SINGLE_ENDED    0U

uint32_t Host_ADC_GetFlag(ADC_HandleTypeDef *hadc, uint32_t flag);
void Host_ADC_ClearFlag(ADC_HandleTypeDef *hadc, uint32_t flag);
#define __HAL_ADC_GET_FLAG(hadc, flag) Host_ADC_GetFlag((hadc), (flag))
#define __HAL_ADC_CLEAR_FLAG(hadc, flag) Host_ADC_ClearFlag((hadc), (flag))

HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *hadc,
                                               uint32_t calibration_mode,
                                               uint32_t single_diff);
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *hadc);
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *hadc);
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *hadc);

void ADC1_DMA_Service(void);
uint8_t ADC1_DMA_ReadLatest(ADC1_DMA_Frame *frame);

#endif
