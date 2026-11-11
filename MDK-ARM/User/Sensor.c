#include "Sensor.h"

#include "adc.h"
#include "fight_config.h"
#include "TIME.h"

typedef struct
{
  GPIO_PinState ir1;
  GPIO_PinState ir2;
  GPIO_PinState ir3;
  GPIO_PinState ir4;
  GPIO_PinState ir5;
  GPIO_PinState ir6;
  GPIO_PinState ir7;
  GPIO_PinState ir8;
  GPIO_PinState ir9;
  GPIO_PinState ir10;
} SensorGpioFrame;

static const uint32_t range_on_threshold[4] =
{
  RANGE1_TARGET_ON,
  RANGE2_TARGET_ON,
  RANGE3_TARGET_ON,
  RANGE4_TARGET_ON
};

static const uint32_t range_off_threshold[4] =
{
  RANGE1_TARGET_OFF,
  RANGE2_TARGET_OFF,
  RANGE3_TARGET_OFF,
  RANGE4_TARGET_OFF
};

static const uint16_t enemy_bit_by_index[6] =
{
  SENSOR_ENEMY_D2,
  SENSOR_ENEMY_D4,
  SENSOR_ENEMY_D5,
  SENSOR_ENEMY_D7,
  SENSOR_ENEMY_D8,
  SENSOR_ENEMY_D10
};

static SensorSnapshot g_sensor_snapshot;
volatile SensorSnapshot g_sensor_debug;
static uint8_t g_enemy_counter[6];
static uint8_t g_range_filter_initialized = 0U;
static uint8_t g_adc2_conversion_active = 0U;
static uint8_t g_adc2_have_sample = 0U;
static uint32_t g_adc2_latest_raw = 0U;
static uint32_t g_adc2_last_update_ms = 0U;
static uint32_t g_adc2_last_start_ms = 0U;
static uint32_t g_adc2_error_count = 0U;
static uint32_t g_sensor_last_publish_ms = 0U;

static SensorGpioFrame Sensor_ReadGpioFrame(void)
{
  SensorGpioFrame frame;

  frame.ir1 = HAL_GPIO_ReadPin(IR1_GPIO_Port, IR1_Pin);
  frame.ir2 = HAL_GPIO_ReadPin(IR2_GPIO_Port, IR2_Pin);
  frame.ir3 = HAL_GPIO_ReadPin(IR3_GPIO_Port, IR3_Pin);
  frame.ir4 = HAL_GPIO_ReadPin(IR4_GPIO_Port, IR4_Pin);
  frame.ir5 = HAL_GPIO_ReadPin(IR5_GPIO_Port, IR5_Pin);
  frame.ir6 = HAL_GPIO_ReadPin(IR6_GPIO_Port, IR6_Pin);
  frame.ir7 = HAL_GPIO_ReadPin(IR7_GPIO_Port, IR7_Pin);
  frame.ir8 = HAL_GPIO_ReadPin(IR8_GPIO_Port, IR8_Pin);
  frame.ir9 = HAL_GPIO_ReadPin(IR9_GPIO_Port, IR9_Pin);
  frame.ir10 = HAL_GPIO_ReadPin(IR10_GPIO_Port, IR10_Pin);

  return frame;
}

static uint8_t Sensor_MakeEdgeMask(const SensorGpioFrame *gpio)
{
  uint8_t mask = 0U;

  /* 图片确认的映射：IR1=左前，IR2=右前，IR8=左后，IR6=右后。 */
  if (gpio->ir1 == GPIO_PIN_SET)
  {
    mask |= SENSOR_EDGE_FRONT_LEFT;
  }
  if (gpio->ir2 == GPIO_PIN_SET)
  {
    mask |= SENSOR_EDGE_FRONT_RIGHT;
  }
  if (gpio->ir8 == GPIO_PIN_SET)
  {
    mask |= SENSOR_EDGE_REAR_LEFT;
  }
  if (gpio->ir6 == GPIO_PIN_SET)
  {
    mask |= SENSOR_EDGE_REAR_RIGHT;
  }

  return mask;
}

static uint16_t Sensor_MakeEnemyRawMask(const SensorGpioFrame *gpio)
{
  uint16_t mask = 0U;

  /* 图片确认的顺时针映射：IR3=2 点、IR4=4 点、IR5=5 点、
     IR7=7 点、IR9=8 点、IR10=10 点；六路输入均为低电平有效。 */
  if (gpio->ir3 == GPIO_PIN_RESET)
  {
    mask |= SENSOR_ENEMY_D2;
  }
  if (gpio->ir4 == GPIO_PIN_RESET)
  {
    mask |= SENSOR_ENEMY_D4;
  }
  if (gpio->ir5 == GPIO_PIN_RESET)
  {
    mask |= SENSOR_ENEMY_D5;
  }
  if (gpio->ir7 == GPIO_PIN_RESET)
  {
    mask |= SENSOR_ENEMY_D7;
  }
  if (gpio->ir9 == GPIO_PIN_RESET)
  {
    mask |= SENSOR_ENEMY_D8;
  }
  if (gpio->ir10 == GPIO_PIN_RESET)
  {
    mask |= SENSOR_ENEMY_D10;
  }

  return mask;
}

static uint16_t Sensor_FilterEnemyDigital(uint16_t raw_mask)
{
  uint32_t index;
  uint16_t filtered_mask = g_sensor_snapshot.enemy_filtered_mask;

  for (index = 0U; index < 6U; index++)
  {
    uint16_t bit = enemy_bit_by_index[index];

    if ((raw_mask & bit) != 0U)
    {
      if (g_enemy_counter[index] < TARGET_DIGITAL_COUNTER_MAX)
      {
        g_enemy_counter[index]++;
      }
    }
    else if (g_enemy_counter[index] > 0U)
    {
      g_enemy_counter[index]--;
    }

    if ((filtered_mask & bit) == 0U)
    {
      if (g_enemy_counter[index] >= TARGET_DIGITAL_ON_COUNT)
      {
        filtered_mask |= bit;
      }
    }
    else if (g_enemy_counter[index] == TARGET_DIGITAL_OFF_COUNT)
    {
      filtered_mask &= (uint16_t)(~bit);
    }
  }

  return filtered_mask;
}

static uint32_t Sensor_EmaStep(uint32_t filtered, uint32_t raw)
{
  const uint32_t rounding = (1U << ADC_RANGE_EMA_SHIFT) - 1U;

  if (raw >= filtered)
  {
    filtered += ((raw - filtered) + rounding) >> ADC_RANGE_EMA_SHIFT;
  }
  else
  {
    filtered -= ((filtered - raw) + rounding) >> ADC_RANGE_EMA_SHIFT;
  }
  return filtered;
}

static void Sensor_UpdateAnalogTargets(SensorSnapshot *snapshot)
{
  uint32_t index;

  if (snapshot->adc1_valid == 0U)
  {
    snapshot->analog_target_mask = 0U;
    return;
  }

  for (index = 0U; index < 4U; index++)
  {
    uint8_t bit = (uint8_t)(1U << index);

    if ((snapshot->analog_target_mask & bit) == 0U)
    {
      if (snapshot->range_filtered[index] >= range_on_threshold[index])
      {
        snapshot->analog_target_mask |= bit;
      }
    }
    else if (snapshot->range_filtered[index] <= range_off_threshold[index])
    {
      snapshot->analog_target_mask &= (uint8_t)(~bit);
    }
  }
}

static void Sensor_ServiceAdc2(uint32_t now_ms)
{
  /* ADC2 only updates private latest-sample state. Snapshot publication
     happens once in Sensor_Service(), preventing old-copy overwrite.

     Important: with ADC_OVR_DATA_PRESERVED an OVR+EOC condition can leave
     the old DR value preserved. Handle OVR first and discard that conversion
     so an overrun value is never stamped as a fresh Range5 sample. */
  if (__HAL_ADC_GET_FLAG(&hadc2, ADC_FLAG_OVR) != 0U)
  {
    __HAL_ADC_CLEAR_FLAG(&hadc2, ADC_FLAG_OVR);
    __HAL_ADC_CLEAR_FLAG(&hadc2, ADC_FLAG_EOC);
    if ((g_adc2_conversion_active != 0U) &&
        (HAL_ADC_Stop(&hadc2) != HAL_OK))
    {
      g_adc2_error_count++;
    }
    g_adc2_error_count++;
    g_adc2_conversion_active = 0U;
  }
  else if ((g_adc2_conversion_active != 0U) &&
           (__HAL_ADC_GET_FLAG(&hadc2, ADC_FLAG_EOC) != 0U))
  {
    g_adc2_latest_raw = HAL_ADC_GetValue(&hadc2);
    g_adc2_last_update_ms = now_ms;
    g_adc2_have_sample = 1U;
    g_adc2_conversion_active = 0U;
  }

  /* A conversion that remains active beyond the freshness window is
     treated as stalled. Do not block the main loop; restart it. */
  if ((g_adc2_conversion_active != 0U) &&
      ((uint32_t)(now_ms - g_adc2_last_start_ms) > ADC2_FRAME_STALE_MS))
  {
    if (HAL_ADC_Stop(&hadc2) != HAL_OK)
    {
      g_adc2_error_count++;
    }
    g_adc2_error_count++;
    g_adc2_conversion_active = 0U;
  }

  if ((g_adc2_conversion_active == 0U) &&
      ((uint32_t)(now_ms - g_adc2_last_start_ms) >= 1U))
  {
    g_adc2_last_start_ms = now_ms;
    if (HAL_ADC_Start(&hadc2) == HAL_OK)
    {
      g_adc2_conversion_active = 1U;
    }
    else
    {
      g_adc2_error_count++;
    }
  }
}

HAL_StatusTypeDef Sensor_Init(void)
{
  uint32_t index;
  HAL_StatusTypeDef status;

  g_sensor_snapshot.tick_ms = 0U;
  g_sensor_snapshot.snapshot_count = 0U;
  g_sensor_snapshot.range5_raw = 0U;
  g_sensor_snapshot.adc1_frame_count = 0U;
  g_sensor_snapshot.adc1_error_count = 0U;
  g_sensor_snapshot.adc1_age_ms = 0xFFFFFFFFU;
  g_sensor_snapshot.adc2_error_count = 0U;
  g_sensor_snapshot.adc2_age_ms = 0xFFFFFFFFU;
  g_sensor_snapshot.enemy_raw_mask = 0U;
  g_sensor_snapshot.enemy_filtered_mask = 0U;
  g_sensor_snapshot.analog_target_mask = 0U;
  g_sensor_snapshot.edge_raw_mask = 0U;
  g_sensor_snapshot.adc1_valid = 0U;
  g_sensor_snapshot.adc2_valid = 0U;

  for (index = 0U; index < 4U; index++)
  {
    g_sensor_snapshot.range_raw[index] = 0U;
    g_sensor_snapshot.range_filtered[index] = 0U;
  }
  for (index = 0U; index < 6U; index++)
  {
    g_enemy_counter[index] = 0U;
  }

  g_range_filter_initialized = 0U;
  g_adc2_conversion_active = 0U;
  g_adc2_have_sample = 0U;
  g_adc2_latest_raw = 0U;
  g_adc2_last_update_ms = 0U;
  g_adc2_error_count = 0U;
  g_adc2_last_start_ms = HAL_GetTick() - 1U;
  g_sensor_last_publish_ms = HAL_GetTick() - FIGHT_CONTROL_PERIOD_MS;

  status = HAL_ADCEx_Calibration_Start(&hadc2,
                                       ADC_CALIB_OFFSET,
                                       ADC_SINGLE_ENDED);
  if (status != HAL_OK)
  {
    g_adc2_error_count++;
    g_sensor_snapshot.adc2_error_count = g_adc2_error_count;
    return status;
  }

  status = HAL_ADC_Start(&hadc2);
  if (status == HAL_OK)
  {
    g_adc2_conversion_active = 1U;
    g_adc2_last_start_ms = HAL_GetTick();
  }
  else
  {
    g_adc2_error_count++;
    g_sensor_snapshot.adc2_error_count = g_adc2_error_count;
  }

  return status;
}

uint8_t Sensor_Service(void)
{
  uint32_t now_ms = HAL_GetTick();
  uint32_t index;
  ADC1_DMA_Frame adc1_frame;
  SensorGpioFrame gpio;
  SensorSnapshot next;

  ADC1_DMA_Service();
  Sensor_ServiceAdc2(now_ms);

  if ((uint32_t)(now_ms - g_sensor_last_publish_ms) < FIGHT_CONTROL_PERIOD_MS)
  {
    return 0U;
  }
  g_sensor_last_publish_ms = now_ms;

  next = g_sensor_snapshot;
  gpio = Sensor_ReadGpioFrame();
  next.tick_ms = now_ms;
  next.snapshot_count++;
  next.edge_raw_mask = Sensor_MakeEdgeMask(&gpio);
  next.enemy_raw_mask = Sensor_MakeEnemyRawMask(&gpio);
  next.enemy_filtered_mask = Sensor_FilterEnemyDigital(next.enemy_raw_mask);

  (void)ADC1_DMA_ReadLatest(&adc1_frame);
  next.adc1_frame_count = adc1_frame.frame_count;
  next.adc1_error_count = adc1_frame.error_count;
  next.adc1_age_ms = now_ms - adc1_frame.last_update_ms;
  next.adc1_valid = ((adc1_frame.valid != 0U) &&
                     (next.adc1_age_ms <= ADC1_FRAME_STALE_MS)) ? 1U : 0U;

  if (next.adc1_valid != 0U)
  {
    for (index = 0U; index < 4U; index++)
    {
      next.range_raw[index] = adc1_frame.range[index];
      if (g_range_filter_initialized == 0U)
      {
        next.range_filtered[index] = adc1_frame.range[index];
      }
      else
      {
        next.range_filtered[index] = Sensor_EmaStep(next.range_filtered[index],
                                                    adc1_frame.range[index]);
      }
    }
    g_range_filter_initialized = 1U;
  }

  next.range5_raw = g_adc2_latest_raw;
  next.adc2_error_count = g_adc2_error_count;
  if (g_adc2_have_sample != 0U)
  {
    next.adc2_age_ms = now_ms - g_adc2_last_update_ms;
    next.adc2_valid =
      (next.adc2_age_ms <= ADC2_FRAME_STALE_MS) ? 1U : 0U;
  }
  else
  {
    next.adc2_age_ms = 0xFFFFFFFFU;
    next.adc2_valid = 0U;
  }

  Sensor_UpdateAnalogTargets(&next);
  g_sensor_snapshot = next;
#if FIGHT_DEBUG
  g_sensor_debug = g_sensor_snapshot;
#endif

  for (index = 0U; index < 4U; index++)
  {
    TIME_SetADCValue((unsigned char)(index + 1U),
                     g_sensor_snapshot.range_filtered[index]);
  }
  TIME_SetADCValue(5U, g_sensor_snapshot.range5_raw);

  return 1U;
}

const SensorSnapshot *Sensor_GetSnapshot(void)
{
  return &g_sensor_snapshot;
}
