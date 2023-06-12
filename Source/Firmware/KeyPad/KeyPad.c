#include <stm32f10x.h>
#include <stm32f10x_gpio.h>
#include <stm32f10x_rcc.h>
#include "KeyPad.h"
//Предусмотреть одновременное нажатие.

//Инициализируем клавиатуру.
void keyPadInit(void)
{
	//RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); Уже включен.
	GPIO_InitTypeDef	gpio;

	gpio.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_6 | GPIO_Pin_5 | GPIO_Pin_4;
	gpio.GPIO_Mode = GPIO_Mode_IPU; //Вход с подтяжкой к питанию.
	gpio.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &gpio);

	//Настраиваем таймер опроса клавиатуры.
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	TIM2->PSC = 30;//2 гц.  //28Частота прерывания 19гц. //549 гц если 1.
	TIM2->DIER |= TIM_DIER_UIE;
	TIM2->CR1 = TIM_CR1_CEN;
	NVIC_EnableIRQ(TIM2_IRQn);
}

//Сканируем клавиатуру.	
void scanKey(uint8_t* keyValue)
{
	//4  бита описывают состояние кнопок,0 нажата. 
	uint16_t a = GPIO_ReadInputData(GPIOB);
	a = a >> 4;
	a = a & 0x0F;

	switch (a)
	{
		//Ни одна кнопка не нажата.
	case 15:
		*keyValue = 0;
		break;

		//Нажата кнопка -. Код 1.
	case 7:
		*keyValue = 1;
		break;

		//Нажата кнопка +. Код 2.
	case 11:
		*keyValue = 2;
		break;

		//Нажата кнопка M. Код 3.
	case 13:
		*keyValue = 3;
		break;

		//Нажата кнопка Exit. Код 1.
	case 14:
		*keyValue = 4;
		break;


	default: *keyValue = 0;
		break;
	}

}

//Сканируем клавиатуру.	Старый тестовый метод для отладки. Не используется. Удалить только в релизе, После полной отладки. 
void scanKey1(uint8_t* keyValue, uint8_t* key_tmp, uint8_t* tickCnt)
{
	if (*tickCnt == 3)
	{
		*tickCnt = 0;
		*keyValue = *key_tmp;
		*key_tmp = 0xff; //Сбрасываем нажатие кнопки.
	}
	else
	{
		*tickCnt += 1;
	}

	//Три младших бита описывают состояние кнопок,0 нажата. 
	uint16_t a = GPIO_ReadInputData(GPIOA);
	a = a & 0x03;
	uint8_t b = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13);
	b = b << 2;
	a = a | b;
	a = a | 0xfff8; //Биты на которых нет кнопок устанавливаем в 1.


	uint16_t out_state = GPIO_ReadInputData(GPIOA);
	out_state = out_state & 0xE0; //Выходы A5-A7.		


	switch (out_state)
	{
		//На А5 низкий уровень. Установлен в ините.
	case 0xc0:
		GPIO_SetBits(GPIOA, GPIO_Pin_5);
		GPIO_ResetBits(GPIOA, GPIO_Pin_6); //A6 level=0.    
		break;

		//На А6 низкий уровень.
	case 0xa0:
		a = a << 3;
		a = a | 0x07; //Устанавливаем младшие три бита в 1.
		GPIO_SetBits(GPIOA, GPIO_Pin_6);
		GPIO_ResetBits(GPIOA, GPIO_Pin_7); //A7 level=0. 		
		break;

		//На А7 низкий уровень.
	case 0x60:
		a = a << 6;
		a = a | 0x3F; //Устанавливаем младшие 6 бит в 1.
		GPIO_SetBits(GPIOA, GPIO_Pin_7);
		GPIO_ResetBits(GPIOA, GPIO_Pin_5); //A5 level=0. 
		break;
	}

	if (a != 0xffff) //Была нажата хотя бы одна кнопка.
		*key_tmp = (uint8_t)a; //Состояние кнопок. 
}
