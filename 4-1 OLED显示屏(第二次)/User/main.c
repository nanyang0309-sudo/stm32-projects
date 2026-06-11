#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "LED.h"
#include "Key.h"

int main(void)
{
	/*模块初始化*/
	OLED_Init();
	LED_Init();
	Key_Init();
	
	uint8_t mode = 0;
	uint8_t temp = 25;
	uint8_t key;
	
	// 开机显示
	OLED_ShowString(1, 1, "mode:0");
	OLED_ShowString(2, 1, "temp:25");
	
	while (1)
	{
		key = Key_GetNum();
		
		// S1：模式切换 0→1→2→0
		if (key == 1)
		{
			mode++;
			if (mode > 2) mode = 0;
			
			if (mode == 0)
			{
				LED1_OFF();
				LED2_OFF();
			}
			else if (mode == 1)
			{
				LED1_ON();
				LED2_ON();
			}
			else if (mode == 2)
			{
				LED1_ON();
				LED2_OFF();
			}
			
			OLED_ShowString(1, 1, "mode:");
			OLED_ShowNum(1, 6, mode, 1);
		}
		
		// S2：温度+1，上限45
		if (key == 2)
		{
			if (temp < 45) temp++;
			OLED_ShowString(2, 1, "temp:");
			OLED_ShowNum(2, 6, temp, 2);
		}
		
		// S3：温度-1
		if (key == 3)
		{
			if (temp > 0) temp--;
			OLED_ShowString(2, 1, "temp:");
			OLED_ShowNum(2, 6, temp, 2);
		}
	}
}
