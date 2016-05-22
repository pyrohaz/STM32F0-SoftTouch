#include "SoftTouch.h"

volatile uint16_t tstart = 0;
volatile uint32_t ncaptures = 0;
volatile SoftTouchStruct tar[ST_NTOUCH];

//User requiring... Unfortunately, all EXTIs are spread over multiple IRQ handlers
//meaning all inputs can't be tested in one IRQ which is a touch annoying. Therefore,
//this required a bit of user modification though I'll be hopefully looking for a solution
//to this soon.

void EXTI0_1_IRQHandler(void){
	if(EXTI_GetITStatus(tar[0].extiline)){
		//Acquire timer count
		tar[0].rawtime = TIM_GetCounter(ST_TIM);
		EXTI_ClearITPendingBit(tar[0].extiline);
	}
}

void EXTI2_3_IRQHandler(void){
	if(EXTI_GetITStatus(tar[1].extiline)){
		//Acquire timer count
		tar[1].rawtime = TIM_GetCounter(ST_TIM);
		EXTI_ClearITPendingBit(tar[1].extiline);
	}
}

//Process a 2 pole low pass filter with Cut F(pole1) = F1 and Cut F(pole2) = F2
//This filter has no feedback and is equivalent to two cascaded one pole filters.
void LP2P_Process(LP2P *lp, int16_t F1, int16_t F2, int16_t Input){
	lp->LP1 += ((Input - lp->LP1)*F1)>>8;
	lp->LP2 += ((lp->LP1 - lp->LP2)*F1)>>8;
}

//Set a pin to an output
void ST_PinOP(GPIO_TypeDef *GPIOx, uint16_t Pin){
	GPIOx->MODER  &= ~(GPIO_MODER_MODER0 << (Pin * 2));
	GPIOx->MODER |= (((uint32_t)GPIO_Mode_OUT) << (Pin * 2));
}

//Set a pin to an input
void ST_PinIP(GPIO_TypeDef *GPIOx, uint16_t Pin){
	GPIOx->MODER  &= ~(GPIO_MODER_MODER0 << (Pin * 2));
	GPIOx->MODER |= (((uint32_t)GPIO_Mode_IN) << (Pin * 2));
}

//Initialize global SoftTouch functionality
void ST_Init(void){
	NVIC_InitTypeDef N;
	TIM_TimeBaseInitTypeDef T;
	GPIO_InitTypeDef G;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM15, ENABLE);

	N.NVIC_IRQChannel = ST_TIRQ;
	N.NVIC_IRQChannelCmd = ENABLE;
	N.NVIC_IRQChannelPriority = 0;
	NVIC_Init(&N);

	T.TIM_Period = 64000-1;
	T.TIM_ClockDivision = TIM_CKD_DIV1;
	T.TIM_Prescaler = 2;
	T.TIM_CounterMode = TIM_CounterMode_Up;
	T.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(ST_TIM, &T);

	TIM_Cmd(ST_TIM, ENABLE);
	TIM_ClearITPendingBit(ST_TIM, TIM_IT_Update);
	TIM_ITConfig(ST_TIM, TIM_IT_Update, ENABLE);

	G.GPIO_Pin = ST_TEN;
	G.GPIO_Mode = GPIO_Mode_OUT;
	G.GPIO_OType = GPIO_OType_PP;
	G.GPIO_PuPd = GPIO_PuPd_NOPULL;
	G.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(ST_EGPIO, &G);
	GPIO_SetBits(ST_EGPIO, ST_TEN);
}

//Initialize a SoftTouch pin
void ST_Config(SoftTouchStruct *S, GPIO_TypeDef *G, uint16_t Pin, uint32_t Line, uint8_t PinSource, uint8_t PortSource, IRQn_Type Irq){
	GPIO_InitTypeDef GS;
	EXTI_InitTypeDef E;
	NVIC_InitTypeDef N;

	//Enable other clocks...
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	SYSCFG_EXTILineConfig(PortSource, PinSource);

	S->extiline = Line;
	S->pinsource = PinSource;
	S->extiportsource = PortSource;
	S->gpio = G;
	S->pin = Pin;

	S->button = 0;
	S->filt.LP1 = ST_DCINIT>>ST_DCSHIFT;
	S->filt.LP2 = ST_DCINIT>>ST_DCSHIFT;
	S->press = 0;
	S->rawtime = 0;
	S->touchdc = ST_DCINIT;

	S->irq = Irq;

	GS.GPIO_Pin = Pin;
	GS.GPIO_OType = GPIO_OType_PP;
	GS.GPIO_Mode = GPIO_Mode_OUT;
	GS.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GS.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(G, &GS);
	GPIO_SetBits(G, Pin);

	EXTI_ClearITPendingBit(Line);
	E.EXTI_Line = Line;
	E.EXTI_Mode = EXTI_Mode_Interrupt;
	E.EXTI_LineCmd = ENABLE;
	E.EXTI_Trigger = EXTI_Trigger_Rising;
	EXTI_Init(&E);

	N.NVIC_IRQChannel = Irq;
	N.NVIC_IRQChannelPriority = 1;
	N.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&N);
}


void TIM15_IRQHandler(void){
	static uint8_t icapt = 0;
	int8_t n;
	uint8_t charge = 0, done = 0;

	if(TIM_GetITStatus(ST_TIM, TIM_IT_Update)){
		TIM_ClearITPendingBit(ST_TIM, TIM_IT_Update);

		for(n = 0; n<ST_NTOUCH; n++){
			//Process
			if(tar[n].rawtime){
				//Subtract timer start time
				tar[n].rawtime -= tstart;

				//Apply 2 pole LPF to raw time
				LP2P_Process(&tar[n].filt, ST_LPFC1, ST_LPFC2, tar[n].rawtime);

				//Remove filter DC and store in press variable
				tar[n].press = tar[n].filt.LP2 - (tar[n].touchdc>>ST_DCSHIFT);

				//Calculate DC value, note the filter value has to be upshifted as finite arithmetic will cause
				//a "stuck" filter without this. Down shifting later on negates this effect so its only required
				//on filter execution.
				tar[n].touchdc += (((tar[n].filt.LP2<<ST_DCSHIFT) - tar[n].touchdc)*ST_DCFC)>>12;

				//If the DC value is larger than the read value, set the DC value to the read value. This
				//is used to reset the DC filter if the button is held for a long time to ensure reads work fine.
				if(tar[n].touchdc > (tar[n].filt.LP2<<ST_DCSHIFT)) tar[n].touchdc = (tar[n].filt.LP2<<ST_DCSHIFT);

				//Hysteresis detection, only allow button pressed to be detected after minimum of
				//ST_CALIBACQ cycles
				if(tar[n].press > ST_UTHRESH && icapt == (ST_CALIBACQ+1)) tar[n].button = 1;
				else if(tar[n].press < ST_LTHRESH) tar[n].button = 0;

				//After ST_CALIBACQ touch acquisitions, set the DC to the current value. This is used to
				//ensure the touch filters have settled enough to set the DC to a useful value. This does
				//however mean that the touch sensors shouldn't be touched at start up to ensure wrong
				//values aren't stored in the DC filters.
				if(icapt == ST_CALIBACQ){
					tar[n].touchdc = tar[n].filt.LP2<<ST_DCSHIFT;
				}
			}
		}

		//Acquisition counter
		if(icapt == ST_CALIBACQ) icapt = ST_CALIBACQ+1;
		else if(icapt<ST_CALIBACQ) icapt++;

		//Capture counter
		ncaptures++;

		//Discharge outputs
		GPIO_ResetBits(ST_EGPIO, ST_TEN);

		for(n = 0; n<ST_NTOUCH; n++){
			ST_PinOP(tar[n].gpio, tar[n].pinsource);
			GPIO_ResetBits(tar[n].gpio, tar[n].pin);
		}

		//Wait for all outputs to go low. While loops in interrupts are normally terrible but
		//this should last no longer than two cycles through the for loop anyway.
		do{
			charge = 0;

			for(n = 0; n<ST_NTOUCH; n++){
				if(GPIO_ReadInputDataBit(tar[n].gpio, tar[n].pin)) charge = 1;
			}

			if(!charge) done = 1;
		} while(!done);

		//Set all outputs to inputs and reset raw touch counters
		for(n = 0; n<ST_NTOUCH; n++){
			ST_PinIP(tar[n].gpio, tar[n].pinsource);
			tar[n].rawtime = 0;
		}

		//Grab timer start time
		tstart = TIM_GetCounter(ST_TIM);

		//Set touch enable pin (set all pull up resistors high).
		GPIO_SetBits(ST_EGPIO, ST_TEN);
	}
}
