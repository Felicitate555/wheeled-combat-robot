/*进场地前先测****************************************************************
	1、将车放台上测四周对地红外对管是否触发
	2、将车放台边和台下测四周对地红外对管是否触发
	6、半身子探出测前后对地红外对管
	3、测台上台下灰度值-并更改“上电上台” “寻台” 1处灰度判断和 ”再上台“ 的2处检台参数
	4、将车放台边，测四个测距模块的极限值-并更改寻人参数
	5、将车放台下，测前两个测距正对适当值-并更改“再上台”中测距正对参数
	
*/

#include "main.h"
#include "adc.h"
#include "tim.h"
#include "gpio.h"

//****************************************************************************
#include "Car.h"
#include "User.h"
#include "Mode.h"
#include "TIME.h"
#include "Sensor.h"

//****************************************************************************
unsigned char User_Mode = 0;

//****************************************************************************

void User_Init(void)
{
	//初始化ADC
	if(ADC1_DMA_Start() != HAL_OK)
	{
		Error_Handler();
	}
	if(Sensor_Init() != HAL_OK)
	{
		Error_Handler();
	}
	
	//电机PWM初始化
	Car_Init();
	
	//初始化定时器
	TIME_Init();
	
	//停车
	Car_SetDirection(Car_Stop);
	
	//进入模式循环
	User_Mode = User_ModeWaitStart;
	
}

void User_While(void)
{
	(void)Sensor_Service();

	if(User_Mode == User_ModeWaitStart)				//非接触式启动
	{
		if(Mode_WaitStart() == 1)User_Mode=User_ModeGoOnStage;
		return;
	}
	else if(User_Mode == User_ModeGoOnStage)		//上电上台
	{
		if(Mode_GoOnStage() == 1)
		{
			Mode_FightEnter();
			User_Mode = User_ModeFinding;
		}
	}
	else if(User_Mode == User_ModeFinding)			//巡场
	{
		if(Mode_Finding() == 1)
		{
			Mode_FightExit();
			User_Mode = User_ModeGoOnStageAgain;
		}
	}
	else if(User_Mode == User_ModeGoOnStageAgain)	//再上台
	{
		if(Mode_GoOnStageAgain() == 1)
		{
			Mode_FightEnter();
			User_Mode = User_ModeFinding;
		}
	}
}



//void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
//{
//    UNUSED(hadc);
//	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 0);
//	
//	Value[0] = HAL_ADC_GetValue(hadc);
//	Value[1] = HAL_ADC_GetValue(hadc);
//	Value[2] = HAL_ADC_GetValue(hadc);
//	Value[3] = HAL_ADC_GetValue(hadc);
//	HAL_ADC_Start_IT(&hadc1);
//}
