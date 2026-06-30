#include "ZW101.h"
#include "Delay.h"

uint8_t USART2_RxBuf[32];
uint8_t USART2_RxFlag = 0;
volatile uint8_t ZW101_TouchFlag = 0; // 触摸标志位初始化

// 将状态机控制变量提升为全局静态变量，以便在清除缓冲区时统一强制复位，杜绝失步
static uint8_t rx_state = 0;   // 状态机步骤
static uint16_t rx_len = 0;    // 本包后续总长度
static uint16_t rx_index = 0;  // 写入缓冲区的索引值

/**
  * @brief  初始化串口2与ZW101通信，PA2 (TX), PA3 (RX)
  * @param  baudrate: 串口波特率 (ZW101 通常默认为 57600)
  * @retval 无
  */
void ZW101_Init(uint32_t baudrate)
{
    // 1. 开启 GPIOA 和 USART2 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    
    // 2. 配置 PA2 (MCU TX) 为复用推挽输出
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // 3. 配置 PA3 (MCU RX) 为上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // 4. 配置 USART2 参数
    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &USART_InitStructure);
    
    // 开启接收中断
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    
    // 5. 配置 NVIC 串口中断优先级
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    USART_Cmd(USART2, ENABLE);
}

/**
  * @brief  ?? 新增：配置 PA4 引脚为触摸感应中断唤醒输入 (接 PIN2 WAKE)
  * @param  无
  * @retval 无
  */
void ZW101_Touch_Init(void)
{
    // 1. 开启 GPIOA 和 AFIO 复用功能时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    
    // 2. 配置 PA4 为下拉输入 (没有触摸时下拉为低电平0V，触摸时高电平3.3V)
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // 3. 将 EXTI 线 4 映射到 GPIOA 4
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource4);
    
    // 4. 配置 EXTI Line 4 (上升沿触发，对应手指碰触瞬间低电平变高电平)
    EXTI_InitTypeDef EXTI_InitStructure;
    EXTI_InitStructure.EXTI_Line = EXTI_Line4;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising; 
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
    
    // 5. 配置 NVIC EXTI4 外部中断优先级
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = EXTI4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1; // 与串口通信优先级相同
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;        // 子优先级排在串口之后
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/**
  * @brief  ?? 新增：读取当前手指是否触摸了指纹模块
  * @param  无
  * @retval 1: 手指已触摸, 0: 手指未触摸
  */
uint8_t ZW101_IsTouched(void)
{
    return (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4) == Bit_SET) ? 1 : 0;
}

/**
  * @brief  ?? 新增：EXTI4 中断服务函数 (处理 PA4 外部上升沿触摸中断)
  */
void EXTI4_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line4) != RESET)
    {
        ZW101_TouchFlag = 1;                // 标记有触摸事件发生
        EXTI_ClearITPendingBit(EXTI_Line4); // 手动清除 EXTI_Line 标志位，防止死循环
    }
}

/**
  * @brief  清空串口接收缓冲区并复位标志、重置接收状态机
  */
void ZW101_ClearRxBuffer(void)
{
    USART2_RxFlag = 0;
    rx_state = 0;     // 强制复位状态机到步骤0
    rx_len = 0;       // 长度计数器清零
    rx_index = 0;     // 缓冲区写入索引清零
    for (uint8_t i = 0; i < 32; i++)
    {
        USART2_RxBuf[i] = 0x00;
    }
}

/**
  * @brief  串口发送数据包
  */
void ZW101_SendCmd(uint8_t *cmd, uint8_t len)
{
    ZW101_ClearRxBuffer(); // 发送前重置接收状态与状态机
    for(uint8_t i = 0; i < len; i++)
    {
        USART_SendData(USART2, cmd[i]);
        while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    }
}

/**
  * @brief  USART2 串口接收中断服务函数
  * 强鲁棒性自愈状态机解析器：自动检测并清除 ORE/FE/NE 等硬件错误，绝不卡死
  */
void USART2_IRQHandler(void)
{
    // 读取状态寄存器以捕获所有可能的中断源与硬件错误标志
    uint16_t sr = USART2->SR;
    
    // 检测是否触发了接收中断(RXNE)或发生了溢出(ORE)、帧错误(FE)、噪声(NE)、奇偶校验错误(PE)
    if (sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE))
    {
        // 读取数据寄存器 DR 这一步操作会【自动清除】RXNE 标志位以及上述所有的 ORE/FE/NE 硬件锁死错误状态
        uint8_t data = (uint8_t)(USART2->DR & 0xFF);
        
        // 只有在确确实实有新数据被成功接收时（RXNE置位），才将有效数据送入状态机解析
        if (sr & USART_SR_RXNE)
        {
            if (rx_state == 0) // 步骤0：寻找包头第一个字节 0xEF
            {
                if (data == 0xEF)
                {
                    USART2_RxBuf[0] = data;
                    rx_state = 1;
                }
            }
            else if (rx_state == 1) // 步骤1：寻找包头第二个字节 0x01
            {
                if (data == 0x01)
                {
                    USART2_RxBuf[1] = data;
                    rx_index = 2;
                    rx_state = 2;
                }
                else
                {
                    rx_state = 0; // 头不对，复位重新寻找
                }
            }
            else if (rx_state == 2) // 步骤2：接收地址(4字节)、包标识PID(1字节)、包长度(2字节)，共7字节
            {
                if (rx_index < 32)
                {
                    USART2_RxBuf[rx_index++] = data;
                }
                else
                {
                    rx_state = 0; // 异常越界保护，复位状态机
                    return;
                }
                
                if (rx_index == 9) // 此时已接收到包长度低字节 (Index 8)
                {
                    // 计算后续“内容+校验和”的总长度
                    rx_len = (USART2_RxBuf[7] << 8) | USART2_RxBuf[8];
                    
                    // 限制安全长度，防溢出 (ZW101基本应答包内容长度通常不超过 23)
                    if (rx_len > 0 && rx_len <= 23) 
                    {
                        rx_state = 3;
                    }
                    else
                    {
                        rx_state = 0; // 长度异常，丢弃此包重来
                    }
                }
            }
            else if (rx_state == 3) // 步骤3：接收剩下的内容数据与校验和
            {
                if (rx_index < 32)
                {
                    USART2_RxBuf[rx_index++] = data;
                }
                else
                {
                    rx_state = 0; // 异常越界保护，复位状态机
                    return;
                }
                
                if (rx_index == (9 + rx_len)) // 完整接收了一包数据
                {
                    USART2_RxFlag = 1; // 标记接收成功，供主循环读取
                    rx_state = 0;      // 复位状态机，等待下一包
                }
            }
        }
    }
}

/**
  * @brief  1. 录入图像 (GetImage)
  */
uint8_t ZW101_GetImage(void)
{
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x01, 0x00, 0x05};
    ZW101_SendCmd(cmd, sizeof(cmd));
    
    // 高精度1ms非阻塞超时等待，模块一旦响应立即跳出，反应速度极快
    uint16_t timeout = 300; // 300ms 超时限额
    while (USART2_RxFlag == 0 && timeout > 0)
    {
        Delay_ms(1);
        timeout--;
    }
    
    if (USART2_RxFlag && USART2_RxBuf[6] == 0x07) 
    {
        return USART2_RxBuf[9]; // 返回模块给的确认码
    }
    return 0xFF; // 通信失败或超时
}

/**
  * @brief  2. 将Image生成特征码并存入指定Buffer (GenChar)
  */
uint8_t ZW101_GenChar(uint8_t bufferId)
{
    uint8_t sum = 0x01 + 0x00 + 0x04 + 0x02 + bufferId;
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x04, 0x02, bufferId, 0x00, sum};
    ZW101_SendCmd(cmd, sizeof(cmd));
    
    // 高精度1ms非阻塞超时等待
    uint16_t timeout = 300;
    while (USART2_RxFlag == 0 && timeout > 0)
    {
        Delay_ms(1);
        timeout--;
    }
    
    if (USART2_RxFlag && USART2_RxBuf[6] == 0x07) 
    {
        return USART2_RxBuf[9];
    }
    return 0xFF;
}

/**
  * @brief  3. 将Buffer1和Buffer2的特征合并生成模板 (RegModel)
  */
uint8_t ZW101_RegModel(void)
{
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x05, 0x00, 0x09};
    ZW101_SendCmd(cmd, sizeof(cmd));
    
    // 高精度1ms非阻塞超时等待
    uint16_t timeout = 300;
    while (USART2_RxFlag == 0 && timeout > 0)
    {
        Delay_ms(1);
        timeout--;
    }
    
    if (USART2_RxFlag && USART2_RxBuf[6] == 0x07) 
    {
        return USART2_RxBuf[9];
    }
    return 0xFF;
}

/**
  * @brief  4. 将合并完成的特征模板存入指定 PageID 存储位 (StoreChar)
  */
uint8_t ZW101_StoreChar(uint16_t pageId)
{
    uint8_t idHigh = (uint8_t)(pageId >> 8);
    uint8_t idLow = (uint8_t)(pageId & 0xFF);
    
    // 动态计算校验和
    uint16_t sumTemp = 0x01 + 0x00 + 0x06 + 0x06 + 0x01 + idHigh + idLow;
    uint8_t sumHigh = (uint8_t)(sumTemp >> 8);
    uint8_t sumLow = (uint8_t)(sumTemp & 0xFF);
    
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x06, 0x06, 0x01, idHigh, idLow, sumHigh, sumLow};
    ZW101_SendCmd(cmd, sizeof(cmd));
    
    // 高精度1ms非阻塞超时等待
    uint16_t timeout = 300;
    while (USART2_RxFlag == 0 && timeout > 0)
    {
        Delay_ms(1);
        timeout--;
    }
    
    if (USART2_RxFlag && USART2_RxBuf[6] == 0x07) 
    {
        return USART2_RxBuf[9];
    }
    return 0xFF;
}

/**
  * @brief  5. 指纹比对检索 (Search)
  */
uint8_t ZW101_Search(uint16_t *matchedPageId, uint16_t *matchScore)
{
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x08, 0x04, 0x01, 0x00, 0x00, 0x00, 0xFF, 0x01, 0x0D};
    ZW101_SendCmd(cmd, sizeof(cmd));
    
    // 高精度1ms非阻塞超时等待 (比对检索由于需要扫255个槽位，给500ms上限，极其稳妥)
    uint16_t timeout = 500;
    while (USART2_RxFlag == 0 && timeout > 0)
    {
        Delay_ms(1);
        timeout--;
    }
    
    if (USART2_RxFlag && USART2_RxBuf[6] == 0x07) 
    {
        uint8_t status = USART2_RxBuf[9];
        if (status == 0x00) // 匹配检索成功
        {
            if (matchedPageId != 0)
            {
                *matchedPageId = (USART2_RxBuf[10] << 8) | USART2_RxBuf[11];
            }
            if (matchScore != 0)
            {
                *matchScore = (USART2_RxBuf[12] << 8) | USART2_RxBuf[13];
            }
        }
        return status;
    }
    return 0xFF;
}

/**
  * @brief  6. 清空模块所有存储指纹 (EmptyLibrary)
  */
uint8_t ZW101_EmptyLibrary(void)
{
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x0D, 0x00, 0x11};
    ZW101_SendCmd(cmd, sizeof(cmd));
    
    // 一键格式化耗时较长，超时时间给 500ms
    uint16_t timeout = 500;
    while (USART2_RxFlag == 0 && timeout > 0)
    {
        Delay_ms(1);
        timeout--;
    }
    
    if (USART2_RxFlag && USART2_RxBuf[6] == 0x07) 
    {
        return USART2_RxBuf[9];
    }
    return 0xFF;
}

/**
  * @brief  7. 删除指定指纹库位置 (DeleteChar)
  */
uint8_t ZW101_DeleteChar(uint16_t pageId)
{
    uint8_t idHigh = (uint8_t)(pageId >> 8);
    uint8_t idLow = (uint8_t)(pageId & 0xFF);
    uint16_t sum = 0x01 + 0x00 + 0x07 + 0x0C + idHigh + idLow + 0x00 + 0x01;
    uint8_t sumHigh = (uint8_t)(sum >> 8);
    uint8_t sumLow = (uint8_t)(sum & 0xFF);
    
    uint8_t cmd[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x07, 0x0C, idHigh, idLow, 0x00, 0x01, sumHigh, sumLow};
    ZW101_SendCmd(cmd, sizeof(cmd));
    
    // 高精度1ms非阻塞超时等待
    uint16_t timeout = 300;
    while (USART2_RxFlag == 0 && timeout > 0)
    {
        Delay_ms(1);
        timeout--;
    }
    
    if (USART2_RxFlag && USART2_RxBuf[6] == 0x07) 
    {
        return USART2_RxBuf[9];
    }
    return 0xFF;
}

/**
  * @brief  录入函数实现
  */
uint8_t ZW101_Enroll(uint16_t fingerId)
{
    uint8_t result;
    
    while(ZW101_GetImage() != 0x00);
    result = ZW101_GenChar(1);
    if(result != 0x00) return result;
    
    while(ZW101_GetImage() == 0x00);
    Delay_ms(500);
    
    while(ZW101_GetImage() != 0x00);
    result = ZW101_GenChar(2);
    if(result != 0x00) return result;
    
    if(ZW101_RegModel() == 0x00)
    {
        if(ZW101_StoreChar(fingerId) == 0x00) 
        {
            return 0x00;
        }
    }
    return 0xFF;
}
