/*
 * onewire.h
 *
 *  Version 1.0.1
 */

#ifndef ONEWIRE_H_
#define ONEWIRE_H_

// для разных процессоров потребуется проверить функцию OW_Init
// на предмет расположения ножек USART
#include "stm32f10x.h"

// выбираем, на каком USART находится 1-wire
//#define OW_USART1
#define OW_USART2
//#define OW_USART3
//#define OW_USART4


// если нужно отдавать тики FreeRTOS, то раскомментировать
//#define OW_GIVE_TICK_RTOS

// первый параметр функции OW_Send
#define OW_SEND_RESET		1
#define OW_NO_RESET		2

// статус возврата функций
#define OW_OK			1
#define OW_ERROR		2
#define OW_NO_DEVICE	3

#define OW_NO_READ		0xff

#define OW_READ_SLOT	0xff

uint8_t OW_Init();
uint8_t OW_Send(uint8_t sendReset, uint8_t *command, uint8_t cLen, uint8_t *data, uint8_t dLen, uint8_t readStart);
uint8_t OW_Scan(uint8_t *buf, uint8_t num);

//Переназначения ножки на время измерения.
//void OW_out_set_as_Power_pin(void);
//void OW_out_set_as_TX_pin(void);

//Устанавливает на шине +3,3 в на время измерения.
void OW_busHi(void);

//Отключает блок подтяжки шины.
void OW_busHiDisable(void);

//Из ардуино.
// Compute a Dallas Semiconductor 8 bit CRC directly.
// this is much slower, but much smaller, than the lookup table.
//
uint8_t ow_crc8( uint8_t *addr, uint8_t len);

#endif /* ONEWIRE_H_ */
