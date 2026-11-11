#ifndef HOST_MAIN_H__
#define HOST_MAIN_H__

#include <stddef.h>
#include <stdint.h>

typedef enum
{
  HAL_OK = 0,
  HAL_ERROR = 1
} HAL_StatusTypeDef;

typedef enum
{
  GPIO_PIN_RESET = 0,
  GPIO_PIN_SET = 1
} GPIO_PinState;



/* Host-only GPIO aliases used by Sensor.c. */
#define IR1_GPIO_Port ((void *)0)
#define IR1_Pin       1U
#define IR2_GPIO_Port ((void *)0)
#define IR2_Pin       2U
#define IR3_GPIO_Port ((void *)0)
#define IR3_Pin       3U
#define IR4_GPIO_Port ((void *)0)
#define IR4_Pin       4U
#define IR5_GPIO_Port ((void *)0)
#define IR5_Pin       5U
#define IR6_GPIO_Port ((void *)0)
#define IR6_Pin       6U
#define IR7_GPIO_Port ((void *)0)
#define IR7_Pin       7U
#define IR8_GPIO_Port ((void *)0)
#define IR8_Pin       8U
#define IR9_GPIO_Port ((void *)0)
#define IR9_Pin       9U
#define IR10_GPIO_Port ((void *)0)
#define IR10_Pin       10U

uint32_t HAL_GetTick(void);
GPIO_PinState HAL_GPIO_ReadPin(void *port, uint16_t pin);

#endif /* HOST_MAIN_H__ */
