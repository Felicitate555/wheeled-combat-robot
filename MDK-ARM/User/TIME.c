#include "main.h"
#include "tim.h"
#include "TIME.h"

volatile unsigned int TIME_CountNum[TIME_AllNum] = {0U};
volatile unsigned char TIME_CountFlag[TIME_AllNum] = {0U};
volatile float Range_Arr[5] = {0.0F};

void TIME_Init(void)
{
  TIME_ClearAll();
  HAL_TIM_Base_Start_IT(&htim3);
}

void TIME_SetTimingMs(unsigned char BitFlag, unsigned int CountNum)
{
  if (BitFlag >= TIME_AllNum)
  {
    return;
  }

  if (TIME_CountFlag[BitFlag] == 0U)
  {
    TIME_CountNum[BitFlag] = CountNum;
    TIME_CountFlag[BitFlag] = 1U;
  }
}

unsigned char TIME_GetTimeOverFlag(unsigned char BitFlag)
{
  if (BitFlag >= TIME_AllNum)
  {
    return 0U;
  }

  if (TIME_CountFlag[BitFlag] == 2U)
  {
    TIME_CountFlag[BitFlag] = 0U;
    return TIME_Over;
  }
  return 0U;
}

void TIME_ClearSetTime(unsigned char BitFlag)
{
  if (BitFlag >= TIME_AllNum)
  {
    return;
  }

  TIME_CountFlag[BitFlag] = 0U;
  TIME_CountNum[BitFlag] = 0U;
}

void TIME_ClearAll(void)
{
  unsigned char index;

  for (index = 0U; index < TIME_AllNum; index++)
  {
    TIME_CountFlag[index] = 0U;
    TIME_CountNum[index] = 0U;
  }
}

float TIME_GetADCValue(unsigned char IRx)
{
  if ((IRx < 1U) || (IRx > 5U))
  {
    return 0.0F;
  }
  return Range_Arr[IRx - 1U];
}

void TIME_SetADCValue(unsigned char IRx, unsigned int Value)
{
  if ((IRx >= 1U) && (IRx <= 5U))
  {
    Range_Arr[IRx - 1U] = (float)Value;
  }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  unsigned char index;

  if ((htim == NULL) || (htim->Instance != TIM3))
  {
    return;
  }

  for (index = 0U; index < TIME_AllNum; index++)
  {
    if ((TIME_CountFlag[index] == 1U) &&
        (TIME_CountNum[index] > 0U))
    {
      TIME_CountNum[index]--;
      if (TIME_CountNum[index] == 0U)
      {
        TIME_CountFlag[index] = 2U;
      }
    }
  }
}
