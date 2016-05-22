/*
 * SoftTouch.h
 *
 *  Created on: 19 May 2016
 *      Author: Haz
 */

#ifndef SOFTTOUCH_H_
#define SOFTTOUCH_H_

#include <stdint.h>
#include <stm32f0xx_gpio.h>
#include <stm32f0xx_rcc.h>
#include <stm32f0xx_tim.h>
#include <stm32f0xx_exti.h>
#include <stm32f0xx_misc.h>
#include <stm32f0xx_syscfg.h>

//Upper press, hysteresis and lower press thresholds
#define ST_UTHRESH	40
#define ST_THIST	10
#define ST_LTHRESH	(ST_UTHRESH - ST_THIST)

//Filter coefficients 2 pole LPF
#define ST_LPFC1	(0.2*256)
#define ST_LPFC2	(0.25*256)

//DC Filter coefficient
#define ST_DCFC		(1)

//Interrupt request numbers
#define ST_TIRQ		TIM15_IRQn

//Hardware timer used
#define ST_TIM		TIM15

#define ST_DCINIT	10000
#define ST_DCSHIFT	6
#define ST_CALIBACQ	20

//Touch enable pin
#define ST_TEN		GPIO_Pin_0
#define ST_EGPIO	GPIOA

#define ST_NTOUCH	2

typedef struct{
	int16_t LP1, LP2;
} LP2P;

typedef struct{
	LP2P filt;
	int16_t rawtime;
	int16_t touchdc;
	uint8_t button;
	int16_t press;

	GPIO_TypeDef *gpio;
	uint16_t pin;
	uint32_t extiline;
	uint8_t pinsource;
	uint8_t extiportsource;

	IRQn_Type irq;

} SoftTouchStruct;

extern volatile SoftTouchStruct tar[ST_NTOUCH];

void ST_Init(void);
void ST_Config(SoftTouchStruct *S, GPIO_TypeDef *G, uint16_t Pin, uint32_t Line, uint8_t PinSource, uint8_t PortSource, IRQn_Type Irq);

#endif /* SOFTTOUCH_H_ */
