#include "stm32f10x.h"   // Device header
#include "Delay.h"

int main(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,ENABLE);
	
	GPIO_InitTypeDef GPIO_Initstructure;
	GPIO_Initstructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Initstructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_Initstructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC,&GPIO_Initstructure);
	while (1)
	{
		GPIO_ResetBits(GPIOC,GPIO_Pin_13);
		Delay_ms(900);
		GPIO_SetBits(GPIOC,GPIO_Pin_13);
		Delay_ms(100);
		
		GPIO_WriteBit(GPIOC,GPIO_Pin_13,Bit_RESET);
		Delay_ms(300);
		GPIO_WriteBit(GPIOC,GPIO_Pin_13,Bit_SET);
		Delay_ms(700);
		
		GPIO_WriteBit(GPIOC,GPIO_Pin_13,(BitAction)0);
		Delay_ms(100);
		GPIO_WriteBit(GPIOC,GPIO_Pin_13,(BitAction)1);
		Delay_ms(900);
	}
}
