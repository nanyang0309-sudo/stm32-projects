#ifndef __ZW101_H
#define __ZW101_H

#include "stm32f10x.h"

// 外部全局状态标志
extern uint8_t USART2_RxBuf[32];
extern uint8_t USART2_RxFlag;
extern volatile uint8_t ZW101_TouchFlag; // 触摸中断触发标志（触摸发生时置1，需手动清0）

// 基础串口配置API
void ZW101_Init(uint32_t baudrate);
void ZW101_ClearRxBuffer(void);
void ZW101_SendCmd(uint8_t *cmd, uint8_t len);

// ?? 新增：PA4 WAKE 触摸唤醒与检测接口
void ZW101_Touch_Init(void);
uint8_t ZW101_IsTouched(void);

// 指纹功能协议API
uint8_t ZW101_GetImage(void);
uint8_t ZW101_GenChar(uint8_t bufferId);
uint8_t ZW101_RegModel(void);
uint8_t ZW101_StoreChar(uint16_t pageId);
uint8_t ZW101_Search(uint16_t *matchedPageId, uint16_t *matchScore);
uint8_t ZW101_EmptyLibrary(void);
uint8_t ZW101_DeleteChar(uint16_t pageId);

// 简易业务录入API
uint8_t ZW101_Enroll(uint16_t fingerId);

#endif
