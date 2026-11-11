#ifndef CAR_H__
#define CAR_H__

#include "Fight.h"

#define Car_Stop		0
#define Car_Forward		1
#define Car_Back		2
#define Car_Lift		3
#define Car_Right		4
#define Car_RightBack	5
#define Car_LowRight	6
#define Car_Befor		10
#define Car_Fight		11

void Car_Init(void);
void Car_SetDirection(unsigned char Direction);
void Car_SetBackSpeed(unsigned char speed);
void Car_SetForwardSpeed(unsigned char speed);
void Car_SetTurnSpeed(unsigned char Direction, unsigned char speed);
void Car_ApplyFightCommand(const FightMotionCommand *command);
void Car_GetSignedOutput(int16_t *left_pwm, int16_t *right_pwm);
unsigned char Car_GetDirection(void);


#endif

