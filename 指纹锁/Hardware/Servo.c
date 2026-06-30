#include "Servo.h"

/**
  * @brief  初始化舵机控制引脚 PA6 (TIM3_CH1)
  * @param  无
  * @retval 无
  */
void Servo_Init(void)
{
    // 1. 开启 TIM3 和 GPIOA 外设时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    // 2. 配置 PA6 极为复用推挽输出 (Alternate Function Push-Pull)
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; 
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // 3. 配置定时器时基 (产生 50Hz, 20ms 周期)
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_Period = 20000 - 1;     // ARR = 19999 (计数20000下)
    TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;     // PSC = 71 (72MHz分频成1MHz，1us计数一次)
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);
    
    // 4. 配置输出比较通道 1 (PWM1 模式)
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 500; // 默认输出 0.5ms 对应的 0 度位置
    TIM_OC1Init(TIM3, &TIM_OCInitStructure);
    
    // 5. 使能 TIM3 定时器
    TIM_Cmd(TIM3, ENABLE);
}

/**
  * @brief  设置舵机旋转角度 (0.5ms~2.5ms 对应 0~180度)
  * @param  Angle: 目标角度，范围 0.0 ~ 180.0
  * @retval 无
  */
void Servo_SetAngle(float Angle)
{
    if (Angle < 0.0f) Angle = 0.0f;
    if (Angle > 180.0f) Angle = 180.0f;
    
    // 映射公式：Compare = 500 + (Angle / 180) * 2000
    // 对应关系: 0° -> 500 (0.5ms); 180° -> 2500 (2.5ms)
    uint16_t compare = (uint16_t)(500 + (Angle / 180.0f) * 2000);
    TIM_SetCompare1(TIM3, compare);
}
