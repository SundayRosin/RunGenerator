#include "stm32f10x.h"
#include "stm32f10x_gpio.h"

//Инициализируем клавиатуру.
void keyPadInit(void);

//Сканируем клавиатуру.
void scanKey(uint8_t* keyValue);

//Старый тестовый метод для отладки.
void scanKey1(uint8_t* keyValue, uint8_t* key_tmp, uint8_t* tickCnt);

