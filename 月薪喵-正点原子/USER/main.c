/**
  ******************************************************************************
  * @file    main.c
  * @author  Gemini built by Google
  * @brief   正点原子 Mini STM32 开发板（2.8寸TFT-LCD）全彩 GIF 动画播放主程序
  * @note    已启用串口初始化以解决 LCD_Init 内部 printf 导致的系统卡死白屏问题。
  ******************************************************************************
  */

// 1. 必须最先包含芯片头文件，否则 delay.h 会报 uint32_t 未定义错误
#include "stm32f10x.h" 

// 2. 包含你工程中实际存在的驱动文件
#include "led.h"   
#include "delay.h" 
#include "usart.h"  // 【核心修复】引入串口驱动头文件
#include "lcd.h"
#include "gif_color_data.h" // 载入由 Python 脚本转换出的彩色猫咪数据

int main(void)
{
    // 用于记录当前播放到动图的第几帧
    uint16_t current_frame = 0;

    // 初始化硬件
    delay_init();             // 延时函数初始化	  
    uart_init(115200);        // 【核心修复】初始化串口，防止 LCD_Init() 里的 printf 卡死系统！
    LED_Init();		  	    // 初始化板载 LED 灯
    LCD_Init();               // 初始化 2.8 寸 TFT-LCD 屏幕

    // 将大屏幕背景刷成纯黑色，以便更美观地凸显猫咪
    LCD_Clear(BLACK);
    
    // 在屏幕上方中央显示说明文字 (参数依次为：X坐标, Y坐标, 宽度, 高度, 字号, 字符串)
    LCD_ShowString(30, 15, 200, 16, 16, "ALIENTEK Mini STM32");
    LCD_ShowString(30, 35, 200, 16, 16, "Playing Colorful Cat...");

    while(1)
    {
        /*
         * 3. 调用正点原子官方驱动里的高速颜色区域填充函数：LCD_Color_Fill
         * 屏幕分辨率为 320x240，猫咪尺寸为 64x64（已进行瘦身压缩）。居中显示坐标计算：
         * X起始 ＝ (320 - 64) / 2 ＝ 128
         * Y起始 ＝ (240 - 64) / 2 ＝ 88
         */
        LCD_Color_Fill(128, 88, 
                       128 + GIF_WIDTH - 1, 
                       88 + GIF_HEIGHT - 1, 
                       (uint16_t*)gif_frames[current_frame]);
        
        // 4. 控制每帧的播放速度，直接使用图片自带的延迟时间（单位：ms）
        delay_ms(gif_delays[current_frame]);
        
        // 5. 切换至下一帧，如果到最后一帧了则循环回第 0 帧
        current_frame++;
        if(current_frame >= GIF_NUM_FRAMES)
        {
            current_frame = 0; 
        }
        
        // 6. 翻转板载指示灯。此时程序不卡死，LED 灯会飞速闪烁
        LED0 = !LED0; 
        LED1 = !LED1;
    }
}
