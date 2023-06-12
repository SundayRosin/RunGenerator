#include <stm32f10x.h>
#include <stm32f10x_gpio.h>
#include <stm32f10x_rcc.h>
#include "stm32f10x_adc.h"
#include "stm32f10x_flash.h"

//Начальный адресс последней страницы в памяти.
#define End_FLASH_PAGE_ADDR 0x800FC00 

const uint8_t prm_word = 4;//Количество слов(323бита) которое занимают настройки в памяти.

void FLASH_Init(void) {
	/* Next commands may be used in SysClock initialization function
	   In this case using of FLASH_Init is not obligatorily */
	   /* Enable Prefetch Buffer */
	FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
	/* Flash 2 wait state */
	FLASH_SetLatency(FLASH_Latency_2);
}

//Читает настройки.
void FLASH_ReadSettings(uint8_t* setings) {
	//Read settings
	uint32_t* source_addr = (uint32_t*)End_FLASH_PAGE_ADDR;
	uint32_t val = 0;

	uint8_t pos = 0;
	for (uint8_t i = 0;i < prm_word;i++) {
		val = *(__IO uint32_t*)source_addr;

		setings[pos] = (uint8_t)(val & 0xff);
		setings[pos + 1] = (uint8_t)((val >> 8) & 0xff);
		setings[pos + 2] = (uint8_t)((val >> 16) & 0xff);
		setings[pos + 3] = (uint8_t)((val >> 24) & 0xff);

		pos += 4;
		source_addr++;
	}
}

//Сохранение пользовательских настроек.
void FLASH_WriteSettings(uint8_t* setings) {

	uint32_t prm[prm_word]; //16 элементов настроек *8=128/32=4.

	//Подготовка данных. Сборка слова.
	uint8_t pos = 0;
	for (uint8_t i = 0;i < prm_word;i++)
	{
		uint32_t a = 0;
		a = (((uint32_t)setings[pos + 3]) << 24) |
			(((uint32_t)setings[pos + 2]) << 16) |
			(((uint32_t)setings[pos + 1]) << 8) |
			(((uint32_t)setings[pos]) << 0);

		pos += 4;
		prm[i] = a;
	}

	// Write settings
	uint32_t* dest_addr = (uint32_t*)End_FLASH_PAGE_ADDR;

	FLASH_Unlock();
	FLASH_ErasePage(End_FLASH_PAGE_ADDR);

	for (uint8_t i = 0;i < prm_word;i++) {
		FLASH_ProgramWord((uint32_t)dest_addr, prm[i]);
		dest_addr++;
	}

	FLASH_Lock();
}