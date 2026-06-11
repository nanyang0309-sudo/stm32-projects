/**
  ******************************************************************************
  * @file    main.c
  * @author  Gemini built by Google
  * @brief   STM32F103C8T6 0.96寸OLED 播放GIF动画高兼容性源码
  * 本代码直接复用你工程中现有的 Delay.h 和 OLED.h，彻底解决重定义报错！
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "OLED.h"   // 引入你工程自带的 OLED 驱动
#include "Delay.h"  // 引入你工程自带的延时函数
#include "gif_data0.h" // 引入你用 Python 生成的数据

// ==========================================
// 声明并复用你已有的 OLED.c 中的底层写指令和写数据函数
// ==========================================
extern void OLED_WriteCommand(uint8_t Command);
extern void OLED_WriteData(uint8_t Data);

/**
  * @brief  OLED高精度寻址定位
  * @param  page: 页地址 (0 ~ 7)
  * @param  col:  列地址 (0 ~ 127)
  */
void OLED_SetPageColumn(uint8_t page, uint8_t col)
{
    OLED_WriteCommand(0xB0 + page);            // 设置页地址
    OLED_WriteCommand(0x10 + (col >> 4));      // 设置列地址高4位
    OLED_WriteCommand(0x00 + (col & 0x0F));    // 设置列地址低4位
}

/**
  * @brief  在OLED上绘制单帧单色图片数据
  * @param  x: 起始X坐标 (0 ~ 127)
  * @param  y: 起始Y坐标 (0 ~ 63，请传入8的倍数)
  * @param  width:  图像宽度 (64)
  * @param  height: 图像高度 (64)
  * @param  frame_data: 指向单帧数组数据的指针
  */
void OLED_DrawGIFFrame(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint8_t *frame_data)
{
    uint8_t page_start = y / 8;
    uint8_t page_count = height / 8;
    
    for (uint8_t p = 0; p < page_count; p++)
    {
        OLED_SetPageColumn(page_start + p, x);
        for (uint8_t col = 0; col < width; col++)
        {
            OLED_WriteData(frame_data[p * width + col]);
        }
    }
}

/* ==========================================
 * 主程序入口
 * ========================================== */
int main(void)
{
    // 1. 初始化你现有的 OLED 屏幕
    OLED_Init();
    OLED_Clear();
    
    while (1)
    {
        // 2. 循环播放 GIF 的每一帧
        for (uint16_t i = 0; i < GIF_NUM_FRAMES; i++)
        {
            // 在屏幕正中央 (32, 0) 位置绘制 64x64 的gif帧
            OLED_DrawGIFFrame(32, 0, GIF_WIDTH, GIF_HEIGHT, gif_frames[i]);
            
            // 延时对应的帧时间 (gif_delays 数组在 gif_dataX.h 中)
            Delay_ms(gif_delays[i]);
        }
    }
}
