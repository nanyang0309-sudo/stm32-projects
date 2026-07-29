#include "LED.h"

/**
  * @brief  初始化LED控制端口 (PA0 和 PA1)
  * @param  无
  * @retval 无
  */
void LED_Init(void)
{
    // 开启 GPIOA 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // ?? 针对高电平点亮的电路：默认输出低电平（0V），让两盏灯初始化时处于熄灭状态
    GPIO_ResetBits(GPIOA, GPIO_Pin_0 | GPIO_Pin_1);
}

/**
  * @brief  开启指纹正确指示灯 (PA0)
  */
void LED_Correct_ON(void)
{
    GPIO_SetBits(GPIOA, GPIO_Pin_0); // 输出高电平点亮
}

/**
  * @brief  关闭指纹正确指示灯 (PA0)
  */
void LED_Correct_OFF(void)
{
    GPIO_ResetBits(GPIOA, GPIO_Pin_0); // 输出低电平熄灭
}

/**
  * @brief  翻转指纹正确指示灯 (PA0)
  */
void LED_Correct_Turn(void)
{
    if (GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_0) == 0)
    {
        GPIO_SetBits(GPIOA, GPIO_Pin_0);
    }
    else
    {
        GPIO_ResetBits(GPIOA, GPIO_Pin_0);
    }
}

/**
  * @brief  开启指纹错误指示灯 (PA1)
  */
void LED_Incorrect_ON(void)
{
    GPIO_SetBits(GPIOA, GPIO_Pin_1); // 输出高电平点亮
}

/**
  * @brief  关闭指纹错误指示灯 (PA1)
  */
void LED_Incorrect_OFF(void)
{
    GPIO_ResetBits(GPIOA, GPIO_Pin_1); // 输出低电平熄灭
}

/**
  * @brief  翻转指纹错误指示灯 (PA1)
  */
void LED_Incorrect_Turn(void)
{
    if (GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_1) == 0)
    {
        GPIO_SetBits(GPIOA, GPIO_Pin_1);
    }
    else
    {
        GPIO_ResetBits(GPIOA, GPIO_Pin_1);
    }
}

/**
  * @brief  同时熄灭两个LED
  */
void LED_All_OFF(void)
{
    GPIO_ResetBits(GPIOA, GPIO_Pin_0 | GPIO_Pin_1); // 全部输出低电平熄灭
}
