#include "stm32f10x.h"
#include "Delay.h"

#define DS18B20_PORT GPIOB
#define DS18B20_PIN GPIO_Pin_14

// 输出模式
void DS18B20_IO_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = DS18B20_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DS18B20_PORT, &GPIO_InitStructure);
}

// 输入模式
void DS18B20_IO_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = DS18B20_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(DS18B20_PORT, &GPIO_InitStructure);
}

// 复位（严格时序）
uint8_t DS18B20_Reset(void)
{
    uint8_t retry = 0;

    DS18B20_IO_OUT();
    GPIO_ResetBits(DS18B20_PORT, DS18B20_PIN);
    Delay_us(480); // 拉低480us

    DS18B20_IO_IN();
    Delay_us(60);  // 释放60us

    while (GPIO_ReadInputDataBit(DS18B20_PORT, DS18B20_PIN) && retry < 200)
    {
        retry++;
        Delay_us(1);
    }

    if (retry >= 200) return 1; // 无应答
    Delay_us(420);
    return 0; // 正常
}

// 读位
uint8_t DS18B20_ReadBit(void)
{
    uint8_t dat;
    DS18B20_IO_OUT();
    GPIO_ResetBits(DS18B20_PORT, DS18B20_PIN);
    Delay_us(2);
    GPIO_SetBits(DS18B20_PORT, DS18B20_PIN);
    DS18B20_IO_IN();
    Delay_us(12);

    dat = GPIO_ReadInputDataBit(DS18B20_PORT, DS18B20_PIN);
    Delay_us(50);
    return dat;
}

// 读字节
uint8_t DS18B20_ReadByte(void)
{
    uint8_t i, j, dat = 0;
    for (i = 0; i < 8; i++)
    {
        j = DS18B20_ReadBit();
        dat = (dat >> 1) | (j << 7);
    }
    return dat;
}

// 写字节
void DS18B20_WriteByte(uint8_t dat)
{
    uint8_t i;
    DS18B20_IO_OUT();
    for (i = 0; i < 8; i++)
    {
        GPIO_ResetBits(DS18B20_PORT, DS18B20_PIN);
        Delay_us(2);

        if (dat & 0x01)
            GPIO_SetBits(DS18B20_PORT, DS18B20_PIN);
        else
            GPIO_ResetBits(DS18B20_PORT, DS18B20_PIN);

        Delay_us(60);
        GPIO_SetBits(DS18B20_PORT, DS18B20_PIN);
        dat >>= 1;
    }
}

// 读取温度（修复：转换后等待750ms）
float DS18B20_GetTemp(void)
{
    uint8_t TH, TL;
    int16_t temp;
    float t;

    if (DS18B20_Reset())
        return -99.9f; // 初始化失败

    DS18B20_WriteByte(0xCC);
    DS18B20_WriteByte(0x44); // 启动转换
    Delay_ms(750); // 12位精度必须等750ms

    if (DS18B20_Reset())
        return -99.9f;

    DS18B20_WriteByte(0xCC);
    DS18B20_WriteByte(0xBE);

    TL = DS18B20_ReadByte();
    TH = DS18B20_ReadByte();

    temp = (int16_t)((TH << 8) | TL);
    t = temp * 0.0625f;

    return t;
}
