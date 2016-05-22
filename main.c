#include <stm32f0xx_gpio.h>
#include <stm32f0xx_rcc.h>
#include "SoftTouch.h"

volatile uint32_t MSec = 0;

void SysTick_Handler(void){
	MSec++;
}

void Delay(uint32_t T){
	uint32_t MSS = MSec;
	while((MSec-MSS)<T);
}

GPIO_InitTypeDef G;
TIM_TimeBaseInitTypeDef T;
EXTI_InitTypeDef E;
NVIC_InitTypeDef N;

int main(void)
{
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOC, ENABLE);

	//LEDs
	G.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
	G.GPIO_Mode = GPIO_Mode_OUT;
	G.GPIO_OType = GPIO_OType_PP;
	G.GPIO_PuPd = GPIO_PuPd_NOPULL;
	G.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOC, &G);

	//Initialize PA1 as a touch input with a large valued resistor (I use 1M) from PA0 to PA1
	ST_Config(&tar[0], GPIOA, GPIO_Pin_1, EXTI_Line1, EXTI_PinSource1, EXTI_PortSourceGPIOA, EXTI0_1_IRQn);
	//Initialize PA2 as a touch input with a large valued resistor (I use 1M) from PA0 to PA2
	ST_Config(&tar[1], GPIOA, GPIO_Pin_2, EXTI_Line2, EXTI_PinSource2, EXTI_PortSourceGPIOA, EXTI2_3_IRQn);
	ST_Init();

	SysTick_Config(SystemCoreClock/1000);

	while(1)
	{
		//Write the button value of the PA1 touch sensor to the onboard LED connected to PC9
		GPIO_WriteBit(GPIOC, GPIO_Pin_9, tar[0].button);

		//Write the button value of the PA2 touch sensor to the onboard LED connected to PC8
		GPIO_WriteBit(GPIOC, GPIO_Pin_8, tar[1].button);
	}
}
