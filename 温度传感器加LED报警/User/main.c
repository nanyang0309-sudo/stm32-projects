#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "DS18B20.h"
#include "LED.h"

int main(void)
{
    float temp;
    int temp_int, temp_float;

    OLED_Init();
    OLED_Clear();
    LED_Init();

    // 固定显示标题
    OLED_ShowString(1, 1, "Temp:");
    OLED_ShowString(2, 9, ".");
    OLED_ShowString(2, 12, "C");

    while (1)
    {
        temp = DS18B20_GetTemp();

        if (temp < -90)
        {
            OLED_ShowString(2, 6, "Err");
            LED1_OFF();
            LED2_OFF();
            Delay_ms(500);
        }
        else
        {
            // 显示温度
            temp_int = (int)temp;
            temp_float = (temp - temp_int) * 100;
            OLED_ShowString(2, 6, "  ");
            OLED_ShowString(2, 10, "  ");
            OLED_ShowNum(2, 6, temp_int, 2);
            OLED_ShowNum(2, 10, temp_float, 2);

            // 温度 >28℃ 进入报警流水灯（保留 while）
            if (temp > 28.0f)
            {
                while (1)
                {
                    // ---------------- 流水灯（超快、无迟钝）----------------
                    LED1_ON();
                    LED2_OFF();
                    Delay_ms(500);  // 短延时，反应快

                    LED1_OFF();
                    LED2_ON();
                    Delay_ms(200);

                    // 每次闪烁都立刻读温度，退出灵敏
                    temp = DS18B20_GetTemp();
                    if (temp <= 28.0f)
                    {
                        LED1_OFF();
                        LED2_OFF();
                        break;
                    }
                }
            }
            else
            {
                LED1_ON();
                LED2_ON();
                Delay_ms(300);
            }
        }
    }
}
