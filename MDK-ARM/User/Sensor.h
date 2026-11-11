#ifndef SENSOR_H__
#define SENSOR_H__

#include "main.h"

/* 底部边缘传感器：高电平表示危险。 */
#define SENSOR_EDGE_FRONT_LEFT     (1U << 0)
#define SENSOR_EDGE_FRONT_RIGHT    (1U << 1)
#define SENSOR_EDGE_REAR_LEFT      (1U << 2)
#define SENSOR_EDGE_REAR_RIGHT     (1U << 3)

/* 数字敌人传感器：实体输入均为低电平有效。 */
#define SENSOR_ENEMY_D2            (1U << 0)
#define SENSOR_ENEMY_D4            (1U << 1)
#define SENSOR_ENEMY_D5            (1U << 2)
#define SENSOR_ENEMY_D7            (1U << 3)
#define SENSOR_ENEMY_D8            (1U << 4)
#define SENSOR_ENEMY_D10           (1U << 5)

/* ADC 敌人传感器按固定 DMA 转换序位排列。 */
#define SENSOR_ANALOG_A11          (1U << 0)
#define SENSOR_ANALOG_A1           (1U << 1)
#define SENSOR_ANALOG_A3           (1U << 2)
#define SENSOR_ANALOG_A9           (1U << 3)

typedef struct
{
  uint32_t tick_ms;
  uint32_t snapshot_count;
  uint32_t range_raw[4];
  uint32_t range_filtered[4];
  uint32_t range5_raw;
  uint32_t adc1_frame_count;
  uint32_t adc1_error_count;
  uint32_t adc1_age_ms;
  uint32_t adc2_error_count;
  uint32_t adc2_age_ms;
  uint16_t enemy_raw_mask;
  uint16_t enemy_filtered_mask;
  uint8_t analog_target_mask;
  uint8_t edge_raw_mask;
  uint8_t adc1_valid;
  uint8_t adc2_valid;
} SensorSnapshot;

extern volatile SensorSnapshot g_sensor_debug;

HAL_StatusTypeDef Sensor_Init(void);
uint8_t Sensor_Service(void);
const SensorSnapshot *Sensor_GetSnapshot(void);

#endif /* SENSOR_H__ */
