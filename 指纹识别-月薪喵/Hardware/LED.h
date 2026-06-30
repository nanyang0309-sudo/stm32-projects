#ifndef __LED_H
#define __LED_H

#include "stm32f10x.h"

void LED_Init(void);
void LED_Correct_ON(void);      // PA0 ÁÁ (Ö¸ÎÆÕýÈ·)
void LED_Correct_OFF(void);     // PA0 Ãð
void LED_Correct_Turn(void);    // PA0 ·­×ª
void LED_Incorrect_ON(void);    // PA1 ÁÁ (Ö¸ÎÆ´íÎó)
void LED_Incorrect_OFF(void);   // PA1 Ãð
void LED_Incorrect_Turn(void);  // PA1 ·­×ª
void LED_All_OFF(void);         // Ë«µÆÏ¨Ãð

#endif
