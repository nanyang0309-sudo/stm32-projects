#include "Key.h"
#include "Delay.h"

/**
  * @brief  按键初始化，配置 PB0, PB10, PB11
  * @param  无
  * @retval 无
  */
void Key_Init(void)
{
    // 启用 GPIOB 端口时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/**
  * @brief  获取按下的键值 (非阻塞+消抖)
  * @param  无
  * @retval 按键序号:
  * 1 -> PB0 按下
  * 2 -> PB10 按下
  * 3 -> PB11 按下
  * 0 -> 无按键按下
  */
uint8_t Key_GetNum(void)
{
    uint8_t KeyNum = 0;
    
    // PB0 检测 (按键1)
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) == 0)
    {
        Delay_ms(20); // 消抖
        while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) == 0); // 等待释放
        Delay_ms(20);
        KeyNum = 1;
    }
    
    // PB10 检测 (按键2)
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10) == 0)
    {
        Delay_ms(20);
        while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10) == 0);
        Delay_ms(20);
        KeyNum = 2;
    }
    
    // PB11 检测 (按键3)
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) == 0)
    {
        Delay_ms(20);
        while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) == 0);
        Delay_ms(20);
        KeyNum = 3;
    }
    
    return KeyNum;
}
