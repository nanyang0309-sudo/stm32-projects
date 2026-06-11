#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "PWM.h"
#include "Key.h"

uint8_t Bright1 = 0;	// PA1 亮度
uint8_t Bright2 = 0;	// PA2 亮度
uint8_t sel = 1;		// 1=控制PA1  2=控制PA2

int main(void)
{
	OLED_Init();
	PWM_Init();
	Key_Init();
	
	// 初始亮度0
	PWM_SetCompare1(Bright1);
	PWM_SetCompare2(Bright2);
	
	while (1)
	{
		uint8_t key = Key_GetNum();
		
		// PB0 = 切换控制灯
		if(key == 1)
		{
			sel = (sel == 1) ? 2 : 1;
		}
		
		// PA10 = 亮度+
		if(key == 2)
		{
			if(sel == 1 && Bright1 < 100) Bright1 += 5;
			if(sel == 2 && Bright2 < 100) Bright2 += 5;
			PWM_SetCompare1(Bright1);
			PWM_SetCompare2(Bright2);
		}
		
		// PA11 = 亮度-
		if(key == 3)
		{
			if(sel == 1 && Bright1 > 0) Bright1 -= 5;
			if(sel == 2 && Bright2 > 0) Bright2 -= 5;
			PWM_SetCompare1(Bright1);
			PWM_SetCompare2(Bright2);
		}
		
		// ========== OLED 显示 ==========
		OLED_Clear();
		OLED_ShowString(1,1,"Sel: LED");
		OLED_ShowNum(1,8,sel,1);
		
		if(sel == 1)
		{
			OLED_ShowString(2,1,"Bright:");
			OLED_ShowNum(2,8,Bright1,3);
		}
		else
		{
			OLED_ShowString(2,1,"Bright:");
			OLED_ShowNum(2,8,Bright2,3);
		}
		
		Delay_ms(50);
	}
}
