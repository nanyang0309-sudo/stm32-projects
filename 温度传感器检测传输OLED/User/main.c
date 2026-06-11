#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "DS18B20.h"

int main(void)
{
    float temp;
    int temp_int, temp_float;

    OLED_Init();
    OLED_Clear();

    // 固定显示标题
    OLED_ShowString(1, 1, "Temp:");
    OLED_ShowString(2, 9, ".");
    OLED_ShowString(2, 12, "C");

    while (1)
    {
        temp = DS18B20_GetTemp();

        if (temp < -90) // 读取失败
        {
            OLED_ShowString(2, 6, "Err");
        }
        else
        {
            temp_int = (int)temp;
            temp_float = (temp - temp_int) * 100;

            // 清除旧数字
            OLED_ShowString(2, 6, "  ");
            OLED_ShowString(2, 10, "  ");

            // 显示新数字
            OLED_ShowNum(2, 6, temp_int, 2);
            OLED_ShowNum(2, 10, temp_float, 2);
        }

        Delay_ms(1000); // 每秒刷新一次
    }
}
