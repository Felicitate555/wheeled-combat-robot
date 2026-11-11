#include "main.h"
#include "tim.h"

#include "Car.h"
#include "fight_config.h"

unsigned char Car_RAM = Car_Stop;

/* 保留原有登台运动职责。已验证的 GPIO 极性和方向含义保持不变；
   战斗速度参数统一放在 fight_config.h 中。 */
unsigned char Car_PWM_Duty[7U * 2U] =
{
  1U, 1U,     /* 停止（禁用方向引脚） */
  50U, 50U,   /* 前进 */
  60U, 60U,   /* 后退 */
  50U, 50U,   /* 左原地旋转 */
  50U, 50U,   /* 右原地旋转 */
  50U, 30U,   /* 原有 RightBack 右后弧线 */
  30U, 30U    /* 原有 LowRight 低速右转 */
};

static int16_t s_last_left_pwm = 0;
static int16_t s_last_right_pwm = 0;

static int16_t Car_ClampPwm(int16_t pwm)
{
  if (pwm > (int16_t)MOTOR_PWM_MAX)
  {
    return (int16_t)MOTOR_PWM_MAX;
  }
  if (pwm < (int16_t)(-((int16_t)MOTOR_PWM_MAX)))
  {
    return (int16_t)(-((int16_t)MOTOR_PWM_MAX));
  }
  return pwm;
}

static uint8_t Car_IsDirectionReversal(int16_t previous_pwm, int16_t next_pwm)
{
  if (((previous_pwm > 0) && (next_pwm < 0)) ||
      ((previous_pwm < 0) && (next_pwm > 0)))
  {
    return 1U;
  }
  return 0U;
}

static void Car_WriteSigned(int16_t left_pwm, int16_t right_pwm)
{
  uint32_t left_duty;
  uint32_t right_duty;

  left_pwm = Car_ClampPwm(left_pwm);
  right_pwm = Car_ClampPwm(right_pwm);

  /* 仅在非零输出真正反向时撤掉对应 PWM。同方向重复命令及同方向调速
     不再每次写入前清零。 */
  if (Car_IsDirectionReversal(s_last_left_pwm, left_pwm) != 0U)
  {
    TIM2->CCR1 = 0U;
  }
  if (Car_IsDirectionReversal(s_last_right_pwm, right_pwm) != 0U)
  {
    TIM2->CCR3 = 0U;
  }

  if (left_pwm > 0)
  {
    HAL_GPIO_WritePin(A1_GPIO_Port, A1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(A2_GPIO_Port, A2_Pin, GPIO_PIN_RESET);
  }
  else if (left_pwm < 0)
  {
    HAL_GPIO_WritePin(A1_GPIO_Port, A1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(A2_GPIO_Port, A2_Pin, GPIO_PIN_SET);
  }
  else
  {
    HAL_GPIO_WritePin(A1_GPIO_Port, A1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(A2_GPIO_Port, A2_Pin, GPIO_PIN_RESET);
  }

  if (right_pwm > 0)
  {
    HAL_GPIO_WritePin(B1_GPIO_Port, B1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(B2_GPIO_Port, B2_Pin, GPIO_PIN_SET);
  }
  else if (right_pwm < 0)
  {
    HAL_GPIO_WritePin(B1_GPIO_Port, B1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(B2_GPIO_Port, B2_Pin, GPIO_PIN_RESET);
  }
  else
  {
    HAL_GPIO_WritePin(B1_GPIO_Port, B1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(B2_GPIO_Port, B2_Pin, GPIO_PIN_RESET);
  }

  left_duty = (left_pwm < 0) ? (uint32_t)(-left_pwm) : (uint32_t)left_pwm;
  right_duty = (right_pwm < 0) ? (uint32_t)(-right_pwm) : (uint32_t)right_pwm;
  TIM2->CCR1 = left_duty;
  TIM2->CCR3 = right_duty;
  s_last_left_pwm = left_pwm;
  s_last_right_pwm = right_pwm;
}

void Car_Init(void)
{
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
  Car_WriteSigned(0, 0);
}

void Car_SetDirection(unsigned char Direction)
{
  int16_t left_pwm;
  int16_t right_pwm;

  if (Direction != Car_Befor)
  {
    Car_RAM = Direction;
  }

  switch (Car_RAM)
  {
    case Car_Stop:
      left_pwm = 0;
      right_pwm = 0;
      break;

    case Car_Forward:
      left_pwm = (int16_t)Car_PWM_Duty[Car_Forward * 2U];
      right_pwm = (int16_t)Car_PWM_Duty[Car_Forward * 2U + 1U];
      break;

    case Car_Back:
      left_pwm = (int16_t)(-((int16_t)Car_PWM_Duty[Car_Back * 2U]));
      right_pwm = (int16_t)(-((int16_t)Car_PWM_Duty[Car_Back * 2U + 1U]));
      break;

    case Car_Lift:
      left_pwm = (int16_t)(-((int16_t)Car_PWM_Duty[Car_Lift * 2U]));
      right_pwm = (int16_t)Car_PWM_Duty[Car_Lift * 2U + 1U];
      break;

    case Car_Right:
      left_pwm = (int16_t)Car_PWM_Duty[Car_Right * 2U];
      right_pwm = (int16_t)(-((int16_t)Car_PWM_Duty[Car_Right * 2U + 1U]));
      break;

    case Car_RightBack:
      left_pwm = (int16_t)(-((int16_t)Car_PWM_Duty[Car_RightBack * 2U]));
      right_pwm = (int16_t)(-((int16_t)Car_PWM_Duty[Car_RightBack * 2U + 1U]));
      break;

    case Car_LowRight:
      left_pwm = (int16_t)Car_PWM_Duty[Car_LowRight * 2U];
      right_pwm = (int16_t)(-((int16_t)Car_PWM_Duty[Car_LowRight * 2U + 1U]));
      break;

    default:
      return;
  }

  Car_WriteSigned(left_pwm, right_pwm);
}

void Car_SetBackSpeed(unsigned char speed)
{
  int16_t back_pwm = Car_ClampPwm((int16_t)speed);

  Car_RAM = Car_Back;
  Car_WriteSigned(-back_pwm, -back_pwm);
}

void Car_SetForwardSpeed(unsigned char speed)
{
  int16_t forward_pwm = Car_ClampPwm((int16_t)speed);

  Car_RAM = Car_Forward;
  Car_WriteSigned(forward_pwm, forward_pwm);
}

void Car_SetTurnSpeed(unsigned char Direction, unsigned char speed)
{
  int16_t turn_pwm = Car_ClampPwm((int16_t)speed);

  if (Direction == Car_Lift)
  {
    Car_RAM = Car_Lift;
    Car_WriteSigned(-turn_pwm, turn_pwm);
  }
  else if (Direction == Car_Right)
  {
    Car_RAM = Car_Right;
    Car_WriteSigned(turn_pwm, -turn_pwm);
  }
  else
  {
    Car_RAM = Car_Stop;
    Car_WriteSigned(0, 0);
  }
}

void Car_ApplyFightCommand(const FightMotionCommand *command)
{
  if (command == NULL)
  {
    Car_RAM = Car_Stop;
    Car_WriteSigned(0, 0);
    return;
  }

  Car_RAM = Car_Fight;
  Car_WriteSigned(command->left_pwm, command->right_pwm);
}

void Car_GetSignedOutput(int16_t *left_pwm, int16_t *right_pwm)
{
  if (left_pwm != NULL)
  {
    *left_pwm = s_last_left_pwm;
  }
  if (right_pwm != NULL)
  {
    *right_pwm = s_last_right_pwm;
  }
}

unsigned char Car_GetDirection(void)
{
  return Car_RAM;
}
