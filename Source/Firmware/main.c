/*
  Для работы загрузчика ардуино нужно настроить Target поставить адреса: 0x8002000  0xE000,
	а также в линкере R/O адрес 0x08002000.
		Подвести мышку к «папке» Stm32Board (слева в дереве проектов).
	Выбрать «Options for Target Stm32Board». Настроить вкладки согласно описанию.
	Также необходимо установить начального адреса вектора прерываний,
	для работы ардуиновского бутлоадера. Все написанное уже настроено в проекте.
*/
#include <stm32f10x.h>
#include <stm32f10x_gpio.h>
#include <stm32f10x_rcc.h>
#include "stm32f10x_adc.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* Подключаем библиотеку и драйвер GPIO */
#include "hd44780.h"
#include "hd44780_stm32f10x.h"

//Библиотека для работы с onewire.
#include "onewire.h"
//Библиотека для работы с часами реального времени.
#include "rtc.h"
//Кнопки.
#include "KeyPad.h"
//АЦП
#include "adc.h"
#include "flash.h"


//-----------Набор переменных и функций необходимых для работы драйвера дисплея.
HD44780 lcd;
HD44780_STM32F10x_GPIO_Driver lcd_pindriver;
void hd44780_assert_failure_handler(const char* filename, unsigned long line);
void init_lcd(void);

void delay_microseconds(uint16_t us);
uint32_t uint32_time_diff(uint32_t now, uint32_t before);

//-----------Конец набора переменных и функций необходимых для работы драйвера дисплея.

//Константы минимального значения параметров системы.
const uint8_t minSettingslim[16] =
{
   3,//15, //Минимальное время задержки перед пуском
   1, //Кол попыток запустить генератор.
   1, //Время вращения стартера секунд	
   1,
   1,

   1,
   1,
   1,
   1,

   1,
   1,
   1,
   1,

   1,
   1,
   0   	//Тест выходов
};

//Константы максимального значения параметров системы.
const uint8_t maxSettingslim[16] = {
	255, //Макс время задержки перед пуском
	20,//Кол попыток запустить генератор.
	30, //Время вращения стартера секунд
	255, 	//Пауза между попытками	 запуска	
	255,
	255,
	255,
	255,
	255,

	255,
	255,
	255,
	255,

	255,
	255,
	12 //Тест выходов   	
};


//Константы по умолчания значения параметров системы.
const uint8_t default_settings_val[16] = {
	30, //Макс время задержки перед пуском,с
	3,//Кол попыток запустить генератор.
	10, //Время вращения стартера секунд
	15, 	//Пауза между попытками	 запуска	
	1,//Damper Cl 
	1,//Damper OP
	1,//7 Not Available
	30,//8 Del Damper T
	1,//9 Coling P

	1,//10 Coling T
	1, //11 Damper Test
	15,//12 on load
	15,//13 off load

	20, //14 Damper Off 20
	10, //15 Full Off
	0//16 Тест выходов   	
};


volatile uint32_t systick_ms = 0;
volatile uint32_t last_systick; //Последний отпечаток системного времени, для не блокирующей задержки.
volatile uint8_t  wait_end_delay = 0; //Флаг для работы метода не блокирующей задержки.

uint8_t keyValue = 0;  //Какая кнопка нажата.
uint8_t lastKeyValue = 0; //Прошлая нажатая клавиша.
uint8_t key_tmp = 0xff;
uint8_t tickCnt = 0;
uint8_t menuMode = 0; //Параметр меню который настраивается. 0-отключено.	
uint8_t menuTimeout = 0; //Счетчик выхода из режима меню, если пользователь бездействует.
uint8_t wasKeyInterupt = 0;//Было прерывание таймера опроса клавиатуры.

uint8_t setingVal[16]; //Параметры настроек.
uint16_t adc_buf = 0;

uint8_t dt_Line = 0; //Состояние датчика электрической сети.

//Входные датчики. [0]-датчик пропадания сети.
uint8_t sensors[6];
uint8_t sensors_cnt[6]; //Программные счетчики задержки срабатывания датчиков. 

uint8_t blink_timer[2] = { 0,0 }; //Программный таймер мигания зведочкой на дисплее.	
const uint8_t blink_del = 15; //Количество прерываний счетчика для моргания.

uint8_t ML_step = 0; //Шаги главной логики.
uint8_t attempt_to_run = 0; //Количество попыток запуска генератора.
uint8_t test_outputs_flag = 0; //Флаг того что был выполнен требуемый тест. Нужно для однократной подачи сигналов на выходы.

//Флаг состояния задвижки
uint8_t damper_flag = 0; //0 не определена. 1-закрыто 2-открыто
//Флаг подключения нагрузки.
uint8_t load_relay_flag = 0; //0 не определена. 1-закрыто 2-открыто
//Флаг включения генератора(поворот ключа)
uint8_t gener_key_flag = 0; //0-откл 1-вкл.

uint32_t cooler_soft_timer_cnt = 0; //Переменная для реализации программного счетчика управления вентилятором.
uint8_t cooler_flag = 0; //Флаг работы вентилятора.

uint32_t cooler_soft_timer_cnt1 = 0; //Переменная для реализации программного счетчика управления вентилятором. Период вращения.


//программный счетчик для формирования минутных интервалов
uint8_t soft_cnt1 = 0;

//Флаг наличия датчика температуры. При единоразовом срабатывание переключается в режим управления вентилятором только по датчику.
uint8_t hasTempSensor = 0;

//Сбрасывает оба программных таймера управления вентилятором.
void reset_cooler_soft_timers()
{
	soft_cnt1 = 0; //Сброс минутного счетчика.
	cooler_soft_timer_cnt = 0; //Переменная для реализации программного счетчика управления вентилятором.
	cooler_flag = 0; //Флаг работы вентилятора.
	cooler_soft_timer_cnt1 = 0; //Переменная для реализации программного счетчика управления вентилятором. Период вращения.

}

//Реализует программный счетчик для сенсоров.
void delay_sensors(uint8_t sensor, uint8_t waitConst)
{
	if (sensors_cnt[sensor] < waitConst) sensors_cnt[sensor]++;
	else
		sensors[sensor] = 1;
}

//Сбрасывает состояние датчика.
void clear_sensor(uint8_t sensor)
{
	sensors_cnt[sensor] = 0;
	sensors[sensor] = 0;
}


//Проверка состояния датчиков.
void check_sensors()
{
	uint16_t a = GPIO_ReadInputData(GPIOA);

	uint8_t delCicles = 15; //Количество прерываний таймера, формирующих задержку принятия решения о срабатывании датчика.

	if ((a & 0x40) == 0) //А6 пропадание напряжения сети. [0]
	{
		delay_sensors(0, delCicles);
	}
	else
	{
		//Сброс состояния датчика.
		clear_sensor(0);
	}

	//А7 датчки темературы [1]
	if ((a & 0x80) == 0)
	{
		delay_sensors(1, delCicles);
	}
	else
	{
		//Сброс состояния датчика.
		clear_sensor(1);
	}


	//А5 датчик 1(A) [2]
	if ((a & 0x20) == 0)
	{
		delay_sensors(2, delCicles);
	}
	else
	{
		//Сброс состояния датчика.
		clear_sensor(2);
	}


	//А4 датчик 2(B) [3]
	if ((a & 0x10) == 0)
	{
		delay_sensors(3, delCicles);
	}
	else
	{
		//Сброс состояния датчика.
		clear_sensor(3);
	}

	//А3 датчик 3(C) [4]
	if ((a & 0x08) == 0)
	{
		delay_sensors(4, delCicles);
	}
	else
	{
		//Сброс состояния датчика.
		clear_sensor(4);
	}


	uint16_t b = GPIO_ReadInputData(GPIOB);
	//B1 датчик работы генератора.
	if ((b & 0x02) == 0)
	{
		delay_sensors(5, delCicles);
	}
	else
	{
		//Сброс состояния датчика.
		clear_sensor(5);
	}

}

//Инициализация входов, которые являются датчиками.
void init_sensors()
{

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_InitTypeDef  GPIO_InitStructure;

	//Датчик напряжения сети.
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;  //Подтянут к земле.
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	//Остальные 4 датчика.
	GPIO_InitTypeDef  GPIO_InitStructure1;
	GPIO_InitStructure1.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_5 | GPIO_Pin_4 | GPIO_Pin_3;
	GPIO_InitStructure1.GPIO_Mode = GPIO_Mode_IPU;  //Подтянут к +3,3в.
	GPIO_InitStructure1.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure1);

	//Датчик работы генератора.
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); //Уже включен.
	GPIO_InitTypeDef  GPIO_InitStructure2;
	GPIO_InitStructure2.GPIO_Pin = GPIO_Pin_1;
	GPIO_InitStructure2.GPIO_Mode = GPIO_Mode_IPU;  //Подтянут к +3,3в.
	GPIO_InitStructure2.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure2);

}

//Исправить аут оф мемори.

//! The interrupt handler for the SysTick module  
void SysTick_Handler(void) {
	systick_ms++;
}


//Вынести в библиотеку.
void delay_us(uint32_t n_usec);


void delay_ms(uint32_t dlyTicks) {
	uint32_t curTicks;
	curTicks = systick_ms;
	while ((systick_ms - curTicks) < dlyTicks) { __NOP(); }
}

//Не блокирующая задержка. Возвращает 1, если задержка сработала.	
uint8_t  delay_ms_nolock(uint32_t dlyTicks) {

	//Все предыдущие задержки обработаны, или их нет.	
	if (wait_end_delay == 0)
	{
		last_systick = systick_ms; //Отпечаток времени.
		wait_end_delay = 1; //Запуск задержки.
	}
	else
	{
		if ((systick_ms - last_systick) < dlyTicks) return 0; //Не сработа задержка.
		else
		{//Сработала.
			systick_ms = 0; //Сброс для предотвращения переполнения переменной.
			last_systick = 0;
			wait_end_delay = 0;
			return 1;
		}

	}

	return 0;
}

//Сброс флагов не блокирующей задержки
void clear_delay_nolock()
{
	wait_end_delay = 0;
	last_systick = 0; //Последний отпечаток системного времени, для не блокирующей задержки.
}

void Delay(volatile uint32_t nCount)
{
	for (uint32_t i = 0; i < nCount; i++);
}


//Задержка в 1 секунду.
void wait_delay_1sec()
{
	Delay(8000000); //Задержка около 1с.
}

uint64_t Uint8ArrtoUint64(uint8_t* var, uint32_t lowest_pos)
{
	return  (((uint64_t)var[lowest_pos + 7]) << 56) |
		(((uint64_t)var[lowest_pos + 6]) << 48) |
		(((uint64_t)var[lowest_pos + 5]) << 40) |
		(((uint64_t)var[lowest_pos + 4]) << 32) |
		(((uint64_t)var[lowest_pos + 3]) << 24) |
		(((uint64_t)var[lowest_pos + 2]) << 16) |
		(((uint64_t)var[lowest_pos + 1]) << 8) |
		(((uint64_t)var[lowest_pos]) << 0);
}


//Считывает из памяти значения.
void loadSettings()
{

	FLASH_ReadSettings(setingVal);

	//Проверка для чистой флеш или поврежденной от времени.
	for (int i = 0;i < 16;i++)
	{
		//Значение в чистой памяти больше предельного.
		if (setingVal[i] > maxSettingslim[i]) setingVal[i] = default_settings_val[i];

		//Значение в памяти меньше минимального
		if (setingVal[i] < minSettingslim[i]) setingVal[i] = default_settings_val[i];

	}

	//Параметр теста выходов не должен сохраняться в памяти.
	setingVal[15] = 0;
}

//Управление заслонкой. 1-открыть.
void control_dapper(uint8_t open)
{
	//Не выполняем если находится в уже таком состоянии.
	if ((open == 0) && (damper_flag == 1)) return;
	if ((open == 1) && (damper_flag == 2)) return;


	char abuf[15];
	if (open == 1)
	{
		abuf[0] = 'O';
		abuf[1] = 'n';
		abuf[2] = ' ';
		abuf[3] = 'D';
		abuf[4] = 'a';
		abuf[5] = 'p';
		abuf[6] = 'p';
		abuf[7] = 'e';
		abuf[8] = 'r';
		abuf[9] = ' ';
		abuf[10] = '\0';


		GPIO_SetBits(GPIOA, GPIO_Pin_0); //A0
		GPIO_SetBits(GPIOC, GPIO_Pin_15);//C15
		hd44780_move_cursor(&lcd, 0, 1); //Вторая строка. 
		hd44780_write_string(&lcd, abuf);

		delay_ms(500);
		GPIO_SetBits(GPIOA, GPIO_Pin_1);//A1
		delay_ms(setingVal[5] * 1000);  //Damper OP T
		GPIO_ResetBits(GPIOA, GPIO_Pin_1);//A1 
		delay_ms(500);
		//Снятие напряжения на реле полярности.			
		GPIO_ResetBits(GPIOA, GPIO_Pin_0); //A0
		GPIO_ResetBits(GPIOC, GPIO_Pin_15);//C15

		damper_flag = 2; //0 не определена. 1-закрыто 2-открыто

	}
	else
	{
		abuf[0] = 'O';
		abuf[1] = 'f';
		abuf[2] = 'f';
		abuf[3] = ' ';
		abuf[4] = 'D';
		abuf[5] = 'a';
		abuf[6] = 'p';
		abuf[7] = 'p';
		abuf[8] = 'e';
		abuf[9] = 'r';
		abuf[10] = ' ';
		abuf[11] = '\0';


		GPIO_ResetBits(GPIOA, GPIO_Pin_0); //A0 //реле полярности.
		GPIO_ResetBits(GPIOC, GPIO_Pin_15);//C15
		hd44780_move_cursor(&lcd, 0, 1); //Вторая строка. 
		hd44780_write_string(&lcd, abuf);

		delay_ms(500);
		GPIO_SetBits(GPIOA, GPIO_Pin_1);//A1
		delay_ms(setingVal[4] * 1000);  //Damper CL T
		GPIO_ResetBits(GPIOA, GPIO_Pin_1);//A1 
		delay_ms(500);

		damper_flag = 1; //0 не определена. 1-закрыто 2-открыто
	}
}

//Формирует импульс зарытия заслонки,если микроконроллер был включен первый раз damper_flag=0;
void fast_close_dapper()
{
	char abuf[15];
	abuf[0] = 'O';
	abuf[1] = 'f';
	abuf[2] = 'f';
	abuf[3] = ' ';
	abuf[4] = 'D';
	abuf[5] = 'a';
	abuf[6] = 'p';
	abuf[7] = 'p';
	abuf[8] = 'e';
	abuf[9] = 'r';
	abuf[10] = ' ';
	abuf[11] = '\0';


	GPIO_ResetBits(GPIOA, GPIO_Pin_0); //A0 //реле полярности.
	GPIO_ResetBits(GPIOC, GPIO_Pin_15);//C15
	hd44780_move_cursor(&lcd, 0, 1); //Вторая строка. 
	hd44780_write_string(&lcd, abuf);

	delay_ms(500);
	GPIO_SetBits(GPIOA, GPIO_Pin_1);//A1
	delay_ms(500);  //Damper OP T
	GPIO_ResetBits(GPIOA, GPIO_Pin_1);//A1 
	delay_ms(500);

	damper_flag = 1; //0 не определена. 1-закрыто 2-открыто
}

//Управление реле подключения нагрузки.
void control_load_relay(uint8_t on)
{
	if (on == 1)
	{
		load_relay_flag = 2;
		GPIO_SetBits(GPIOA, GPIO_Pin_8);//A8
	}
	else
	{
		load_relay_flag = 1;
		GPIO_ResetBits(GPIOA, GPIO_Pin_8); //A8 
	}
}

//Реализует моргание требуемыми символами.
//*buf начальная позиция.
//len количество символов с которыми работаем.
void blinkSimvols(char* buf, uint8_t len)
{
	char ch = ' ';
	if (blink_timer[0] > 0) ch = '.';

	for (int i = 0;i < len;i++)
	{
		*buf = ch;
		buf++;
	}
}

//Заглушить двигатель. Если не смог заглушить возвращается 1.
uint8_t stopRotor()
{
	char buf[17] = "               ";//15пробелов
	buf[0] = 'S';
	buf[1] = 't';
	buf[2] = 'o';
	buf[3] = 'p';
	buf[4] = ' ';
	buf[5] = 'G';
	buf[6] = 'e';
	buf[7] = 'n';
	buf[8] = 'e';
	buf[9] = 'r';
	buf[10] = 'a';
	buf[11] = 't';
	buf[12] = 'o';
	buf[13] = 'r';
	buf[14] = '.';
	buf[15] = '\0';
	blinkSimvols(&buf[14], 1);//Моргаем точками.

	hd44780_move_cursor(&lcd, 0, 1); //Вторая строка.
	hd44780_write_string(&lcd, buf);

	GPIO_SetBits(GPIOA, GPIO_Pin_2);//A2 
	delay_ms(5000);

	gener_key_flag = 0; //0-откл 1-вкл.
	GPIO_ResetBits(GPIOA, GPIO_Pin_2); //A2 

	//Проверяю остановился ли генератор.
	if (sensors[5] == 1) return 1; //Не остановился.

	return 0;
}

//uint8_t test_outputs_flag=0; //Флаг того что был выполнен требуемый тест. Нужно для однократной подачи сигналов на выходы.
//Выполняет тест выходов.
void work_test_outputs()
{
	uint8_t cur_val = setingVal[15]; //Текущее значние.

	if (setingVal[15] >= 12) setingVal[15] = 0; //Циклическая смена значения.

	if (test_outputs_flag != cur_val) //Изменено значение.
	{
		switch (cur_val)
		{
		case 1:
			control_dapper(0); //Возврат заслонки в исходное положение.
			break;

		case 2:
			GPIO_SetBits(GPIOA, GPIO_Pin_10);//Подача топлива.
			delay_ms(300);
			break;

		case 3:
			GPIO_SetBits(GPIOC, GPIO_Pin_14);//Включение стартера.
			delay_ms(2000);
			GPIO_ResetBits(GPIOC, GPIO_Pin_14);//Включение стартера.
			break;

		case 4:
			control_dapper(1); //Открыть заслонку.
			break;

		case 5:
			control_load_relay(1); //Включение нагрузки.
			break;

			//6-включить вентилятор
		case 6:
			GPIO_SetBits(GPIOA, GPIO_Pin_9);//Включение вентилятора.
			break;

			//7-отключить вентилятор
		case 7:
			GPIO_ResetBits(GPIOA, GPIO_Pin_9); //Отключение вентилятора.
			break;

		case 8:
			control_load_relay(0); //Отключение нагрузки.
			break;

		case 9:
			control_dapper(0); //Возврат заслонки в исходное положение.
			break;

		case 10:
			GPIO_ResetBits(GPIOA, GPIO_Pin_10);//Отключить топливный клапан.
			break;

		case 11:
			stopRotor(); //Заглущить двигатель.
			break;

		default:
			break;
		}

		test_outputs_flag = cur_val;
	}
}

//Проверка минимального значения параметров системы.
uint8_t checkMinSettingValue(uint8_t menu, uint8_t val)
{
	//Достигнут предел. 
	if (val <= minSettingslim[menu]) return 0;

	return 1;
}

//Проверка максимального значения.
uint8_t checkMaxSettingValue(uint8_t menu, uint8_t val)
{
	//Достигнут предел. 
	if (val >= maxSettingslim[menu]) return 0;

	return 1;
}

//В первой строке отображает название пункта меню.
void showMenuDisplay(uint8_t menu)
{
	if (menu == 0) return;//Режим меню отключен.

	menu = menu - 1; //Для выбора из массива.

	//11 пунктов
	char  names[16][16] = {
		{"<Del begin run> "}, //[0] 16 символов
		{"<Num attempts>  "},//[1]
		{"<Starter Time>  "},//[2]
		{"<Pause S Time>  "},//[3]
		{"<Damper CL T>   "},	//[4]
		{"<Damper OP T>   "},	//[5]
		{"<Not available> "},	//[6]
		{"<Del Damper T>  "},	//[7]
		{"<Cooling P>    "},	//[8]
		{"<Cooling T>    "}, //[9] 10	
		{"<Damper Test>   "}, //[10]
		{"<On Load>       "}, //[11]
		{"<Off Load>      "}, //[12]
		{"<Damper off>    "}, //[13]
		{"<Full off>      "}, //[14]
		{"<Test outputs>  "} //[15]	 
	};

	char abuf[16];
	abuf[15] = '\0';

	//Копируем текущее название пункта меню.
	for (int i = 0;i < 15;i++)  abuf[i] = names[menu][i];

	hd44780_move_cursor(&lcd, 0, 0);
	hd44780_write_string(&lcd, abuf); //Вывожу название параметра.

	if ((keyValue == 1) && (checkMinSettingValue(menu, setingVal[menu]) == 1)) setingVal[menu]--; //Уменьшение значения.
	if ((keyValue == 2) && (checkMaxSettingValue(menu, setingVal[menu]) == 1)) setingVal[menu]++; //Увеличение значения.

	//Отображение значений параметра. 	
	sprintf(abuf, "%d", setingVal[menu]);

	//Добавляю пробелы после цифр.		 
	uint8_t	find = 0;
	for (uint8_t i = 0;i < 15;i++)
	{
		if (abuf[i] == '\0') find = 1; //После цифр идет конец строки.
		if (find == 1)abuf[i] = ' '; //Очистка старого дисплея.	
	}

	abuf[15] = '\0';

	//Пользователь сохраняет настройки.
	if (keyValue == 4)
	{
		abuf[0] = 'S';
		abuf[1] = 'A';
		abuf[2] = 'V';
		abuf[3] = 'E';
		abuf[4] = '.';
		abuf[5] = '.';
		abuf[6] = '.';

		hd44780_move_cursor(&lcd, 0, 1); //Вторая строка. 
		hd44780_write_string(&lcd, abuf);
		FLASH_WriteSettings(setingVal);
	}

	hd44780_move_cursor(&lcd, 0, 1); //Вторая строка. 
	hd44780_write_string(&lcd, abuf);

	//Включен режим тестирования выходов.
	if (menu == 15) work_test_outputs();
}

//Отображает информацию о датчиках.
void display_sensors(char* buf)
{
	char simvols[6] = { 'L','T','K','B','C','G' }; //Символическое отображение каждого датчика.

	for (uint8_t i = 0;i < 6;i++)
	{
		if (sensors[i] > 0) buf[i] = simvols[i];
	}
}

//Отображает процессы работы алгоритма. Если не требуется отображение шага алгоритма, тогда возвращает 0.
uint8_t showStep()
{
	//Заполнить пробелами вместо очистки.
	hd44780_move_cursor(&lcd, 0, 1); //Вторая строка.

	uint8_t needShow = 0; //Необходимо выводить информацию.
	char buf[17] = "               ";//15пробелов

	switch (ML_step)
	{

		//Работа схемы заблокирована ключем.
	case 0:
		if (sensors[2] == 1) //Работа системы заблокирована ключем.  
		{
			buf[0] = 'K';buf[1] = 'E';buf[2] = 'Y';buf[3] = ' ';buf[4] = 'I';buf[5] = 'S';buf[6] = ' ';buf[7] = 'O';buf[8] = 'F';buf[9] = 'F';buf[10] = '!';buf[11] = ' '; buf[12] = ' ';buf[13] = ' ';
			buf[14] = ' ';buf[15] = '\0';
			blinkSimvols(&buf[11], 2);//Моргаем точками.
			needShow = 1;
		}
		break;

		//Wait to Run
	case 1:
		buf[0] = 'W';buf[1] = 'a';buf[2] = 'i';buf[3] = 't';buf[4] = ' ';buf[5] = 't';buf[6] = 'o';buf[7] = ' ';buf[8] = 'R';buf[9] = 'u';buf[10] = 'n';buf[11] = '.'; buf[12] = '.';buf[13] = '.';
		buf[14] = ' ';buf[15] = '\0';
		blinkSimvols(&buf[11], 3);//Моргаем точками.
		needShow = 1;
		break;

		//Starter вращение стартера. 
	case 2:
		buf[0] = 'S';buf[1] = 't';buf[2] = 'a';buf[3] = 'r';buf[4] = 't';buf[5] = 'e';buf[6] = 'r';buf[7] = '.';buf[8] = '.';buf[9] = '.';buf[10] = ' ';buf[11] = ' '; buf[12] = ' ';buf[13] = ' ';
		buf[14] = ' ';buf[15] = '\0';
		blinkSimvols(&buf[7], 3);//Моргаем точками.		 
		needShow = 1;
		break;

		//Пауза перед повторным пуском. 
	case 3:
		buf[0] = 'S';buf[1] = 't';buf[2] = 'a';buf[3] = 'r';buf[4] = 't';buf[5] = 'e';buf[6] = 'r';buf[7] = ' ';buf[8] = 'P';buf[9] = 'a';buf[10] = 'u';buf[11] = 's'; buf[12] = 'e';buf[13] = ' ';
		buf[14] = ' ';buf[15] = '\0';
		blinkSimvols(&buf[13], 2);//Моргаем точками.		 
		needShow = 1;
		break;

		//Wait Damper
	case 4:
		buf[0] = 'W';buf[1] = 'a';buf[2] = 'i';buf[3] = 't';buf[4] = ' ';buf[5] = 'D';buf[6] = 'a';buf[7] = 'm';buf[8] = 'p';buf[9] = 'e';buf[10] = 'r';buf[11] = '.'; buf[12] = '.';buf[13] = '.';
		buf[14] = ' ';buf[15] = '\0';
		blinkSimvols(&buf[11], 3);//Моргаем точками.
		needShow = 1;
		break;

		//«Wait OnLoad»;
	case 5:
		buf[0] = 'W';buf[1] = 'a';buf[2] = 'i';buf[3] = 't';buf[4] = ' ';buf[5] = 'O';buf[6] = 'n';buf[7] = 'L';buf[8] = 'o';buf[9] = 'a';buf[10] = 'd';buf[11] = '.'; buf[12] = '.';buf[13] = '.';
		buf[14] = ' ';buf[15] = '\0';
		blinkSimvols(&buf[11], 3);//Моргаем точками.
		needShow = 1;
		break;

		//«Wait Off OnLoad»; или режим постоянной работы.
	case 6:
		if (sensors[0] == 1) //Нет напряжения в сети.
		{
			buf[0] = 'W';buf[1] = 'O';buf[2] = 'R';buf[3] = 'K';buf[4] = ' ';buf[5] = 'G';buf[6] = ' ';buf[7] = ' ';buf[8] = ' ';buf[9] = ' ';buf[10] = ' ';buf[11] = ' '; buf[12] = ' ';buf[13] = ' ';
			buf[14] = ' ';buf[15] = '\0';
			blinkSimvols(&buf[7], 3);//Моргаем точками.
		}
		else //Есть.
		{
			buf[0] = 'W';buf[1] = 'a';buf[2] = 'i';buf[3] = 't';buf[4] = ' ';buf[5] = 'O';buf[6] = 'f';buf[7] = 'f';buf[8] = ' ';buf[9] = 'L';buf[10] = 'o';buf[11] = 'a'; buf[12] = 'd';buf[13] = '.';
			buf[14] = '.';buf[15] = '\0';
			blinkSimvols(&buf[13], 2);//Моргаем точками.
		}
		needShow = 1;
		break;

		//«Wait Damper off»;
	case 7:
		if (sensors[0] != 0) return 0; //Нет напряжения в сети-шаг не выполняем.
		buf[0] = 'W';buf[1] = 'a';buf[2] = 'i';buf[3] = 't';buf[4] = ' ';buf[5] = 'D';buf[6] = 'a';buf[7] = 'm';buf[8] = 'p';buf[9] = 'e';buf[10] = 'r';buf[11] = 'O'; buf[12] = 'f';buf[13] = 'f';
		buf[14] = ' ';buf[15] = '\0';
		blinkSimvols(&buf[14], 1);//Моргаем точками.
		needShow = 1;
		break;


	case 8:
		if (sensors[0] != 0) return 0; //Нет напряжения в сети-шаг не выполняем.
		buf[0] = 'W';buf[1] = 'a';buf[2] = 'i';buf[3] = 't';buf[4] = ' ';buf[5] = 'F';buf[6] = 'u';buf[7] = 'l';buf[8] = 'l';buf[9] = ' ';buf[10] = 'O';buf[11] = 'f'; buf[12] = 'f';buf[13] = ' ';
		buf[14] = ' ';buf[15] = '\0';
		blinkSimvols(&buf[13], 2);//Моргаем точками.
		needShow = 1;
		break;

		//Ошибка запуска генератора.
	case 250:
		buf[0] = 'E';buf[1] = 'r';buf[2] = 'r';buf[3] = 'o';buf[4] = 'r';buf[5] = ' ';buf[6] = 'S';buf[7] = 't';buf[8] = 'a';buf[9] = 'r';buf[10] = 't';buf[11] = 'e'; buf[12] = 'r';buf[13] = ' ';
		buf[14] = ' ';buf[15] = '\0';
		blinkSimvols(&buf[13], 2);//Моргаем точками.
		needShow = 1;
		break;

		//Ошибка остановки генератора. //Error TurnOff G
	case 251:
		buf[0] = 'E';buf[1] = 'r';buf[2] = 'r';buf[3] = 'o';buf[4] = 'r';buf[5] = ' ';buf[6] = 'T';buf[7] = 'u';buf[8] = 'r';buf[9] = 'n';buf[10] = 'O';buf[11] = 'f'; buf[12] = 'f';buf[13] = ' ';
		buf[14] = 'G';buf[15] = '\0';
		blinkSimvols(&buf[14], 1);//Моргаем точками.
		needShow = 1;
		break;

	default:
		return 0;
	}

	//Нужно выводить информацию.
	if (needShow == 1)
	{
		hd44780_move_cursor(&lcd, 0, 1); //Вторая строка.
		hd44780_write_string(&lcd, buf);
		return 1;
	}

	return 0;
}

//Отображение дисплея по умолчанию.
void showDefaultDisplay()
{
	//Верхняя строка.	
	char buf[17] = "               ";//15пробелов
	display_sensors(buf);// Получение информации о датчиках.
	buf[15] = '\0';

	hd44780_move_cursor(&lcd, 0, 0);
	hd44780_write_string(&lcd, buf); //Вывожу название параметра.

	//Требуется только отобразить информацию о шагах работы.
	if (showStep() == 1) return;

	//Заполнить пробелами вместо очистки.
	hd44780_move_cursor(&lcd, 0, 1); //Вторая строка.

	buf[0] = 'M';
	buf[1] = 'o';
	buf[2] = 'n';
	buf[3] = 'i';
	buf[4] = 't';
	buf[5] = 'o';
	buf[6] = 'r';
	buf[7] = 'i';
	buf[8] = 'n';
	buf[9] = 'g';
	buf[10] = ' ';

	blinkSimvols(&buf[11], 3); //Моргаю точками.

	buf[14] = '\0';

	hd44780_write_string(&lcd, buf);
}

//Управление вентилятором по таймеру.
void control_cooler()
{
	//Период охлаждения. 	
	if (cooler_flag == 0) //Вентилятор не работает.
	{
		if (cooler_soft_timer_cnt < setingVal[8])cooler_soft_timer_cnt++;
		else
		{
			cooler_soft_timer_cnt = 0;
			if ((ML_step >= 6) && (ML_step <= 200)) //Генератор работает. Нет критических ошибок. Нагрузка подключена.
			{
				GPIO_SetBits(GPIOA, GPIO_Pin_9); //Включаем вентилятор.				
				cooler_flag = 1; //Включем вентилятор.
			}
		}
	}
	else
	{
		//Формирование времени работы.
		if (cooler_soft_timer_cnt1 < setingVal[9])cooler_soft_timer_cnt1++;
		else
		{
			cooler_soft_timer_cnt1 = 0;
			cooler_soft_timer_cnt = 0; //Сброс осчета периода.
			cooler_flag = 0;
			GPIO_ResetBits(GPIOA, GPIO_Pin_9); //Выключаем вентилятор.	
		}
	}
}


//Управление вентилятором по датчику.
void control_cooler_sensor()
{
	if (sensors[1] == 1)  GPIO_SetBits(GPIOA, GPIO_Pin_9); //Включаем вентилятор.	
	else
		GPIO_ResetBits(GPIOA, GPIO_Pin_9); //Выключаем вентилятор.	
}

//Прерывания от таймера опроса клавиатуры.
void TIM2_IRQHandler(void)
{
	TIM2->SR &= ~TIM_SR_UIF;
	scanKey(&keyValue);
	wasKeyInterupt = 1;
	//Проверка состояния датчиков.
	check_sensors();

	//Программный таймер мигания. Будем считать что срабатывает раз в пол секунды.
	if (blink_timer[1] < blink_del) blink_timer[1]++;
	else
	{
		blink_timer[1] = 0;
		blink_timer[0] = ~blink_timer[0];

		//Флаг наличия датчика температуры. При единоразовом срабатывание переключается в режим управления вентилятором только по датчику.
		if (hasTempSensor == 0)
		{
			if (sensors[1] == 1) hasTempSensor = 1; //Сработал датчик, значит дальнейшая работа только по датчику.

			//программный счетчик для формирования минутных интервалов. 1с=0,875
			if (soft_cnt1 < 67) soft_cnt1++;
			else
			{
				soft_cnt1 = 0;
				control_cooler(); //Логика управления вентилятором.
			}
		}
		else
		{
			//Управление вентилятором только по датчику.
			control_cooler_sensor();
		}
	}
}

/*
   Конвертируем массив в строку.
	 in-входящий массив;
		 hex_str-выходной массив,
		begPos-начальная позиция во входном массиве,endPos-конечная позиция во входном массиве.
*/
void toHex(uint8_t* in, char* hex_str, int begPos, int endPos)
{
	for (int x = begPos; x < endPos; x++)
		sprintf(hex_str + (x * 2), "%02x", in[x]);
}


//Логика обработки нажатия на кнопки.
void ui_logik()
{
	//Пользователь что то кнопал, уменьшаем тайм аут выхода из меню.
	if (menuTimeout > 0) menuTimeout--;

	if (keyValue > 0)	menuTimeout = 40; //Пользователь что то кнопает, обновляем счетчик тайм аута.

	//Нажата кнопка -. Код 1. +. Код 2.  M. Код 3. Exit. Код 4.

	   //Нажата кнопка меню. Код 3.
	if (keyValue == 3)	lastKeyValue = 3;


	//Реализация реакции меню по нажатию и отпусканию кнопки.
	if ((lastKeyValue == 3) && (keyValue == 0))
	{
		lastKeyValue = 0;
		if (menuMode <= 15)
		{
			menuMode++; //Включаем меню или меняем по кругу.
			menuTimeout = 45; //Вход в процедуру равен тику таймера.
		}
		else menuMode = 1;
	}

	//В режиме теста выходов не выходим из меню.
	if (menuMode == 16) menuTimeout = 45;

	if (menuTimeout == 0) menuMode = 0; //Истек тайм аут бездействи пользователя, отключаем режим меню.

	//Режим меню отключен.
	if (menuMode == 0)
	{
		showDefaultDisplay();
	}
	else
		showMenuDisplay(menuMode); //Отображаю пункт меню если режим меню включен.)

}

//Инициализация входов, которые являются датчиками.
void init_out()
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_InitTypeDef  GPIO_InitStructure1;

	GPIO_InitStructure1.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_9 | GPIO_Pin_8 | GPIO_Pin_2 | GPIO_Pin_1 | GPIO_Pin_0;
	GPIO_InitStructure1.GPIO_Mode = GPIO_Mode_Out_PP;  //Подтянут к земле.
	GPIO_InitStructure1.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure1);
}

//Оключение вcех выходов. Используется если ключ повернут в положение Off.
void offAllOut()
{
	GPIO_ResetBits(GPIOC, GPIO_Pin_14); //Отключение стартера,если был включен. 
	GPIO_ResetBits(GPIOA, GPIO_Pin_10);//Нет подачи топлива.
	control_dapper(0);//Закрываем заслонку.	Если она не была закрыта(по факту не сработает так как в нутри функции стоит проверка флага заслонки)
	control_load_relay(0);
	GPIO_ResetBits(GPIOA, GPIO_Pin_9);	//Откл вентилятор.
	reset_cooler_soft_timers(); //Сброс программного таймера управления вентилятором.

	clear_delay_nolock(); //Сброс флагов не блокирующей задержки.
	attempt_to_run = 0; //Сброс количества попыток.
	ML_step = 0;
}


//Основная логика работы.
void ML_Logic()
{
	//Ключ OFF повернут
	if (sensors[2] == 1)
	{
		ML_step = 0; //Сброс ошибок если они были. Нужно повернуть ключем.	

		//Если генератора работает пытаемся его заглушить.
		if (sensors[5] == 1)
		{
			stopRotor();
			ML_step = 0; //Что бы не пытался повторно запустить цикл.
			delay_ms(10000);
		}
	}

	//Мы только включились, генератор уже работает, ключ в режиме ON
	if ((ML_step == 0) && (sensors[5] == 1))
	{
		//Если ключ не в режиме OFF.
		if (sensors[2] == 0) GPIO_SetBits(GPIOA, GPIO_Pin_10);//Включить топливный клапан.
		ML_step = 5; //Шаг подключения нагрузки.
	}

	//Если генератор заглох,сбросим флаги, вернем заслонку. пробуем повторно завести.

	 //Пропало напряжение в сети.
	if ((ML_step == 0) && (sensors[0] == 1))
	{
		//Ключ в положении Off. Не возможно запустить генератор.
		if (sensors[2] == 1)
		{
			offAllOut();
			return;
		}

		ML_step = 1; //Шаг алгоритма-Задержка перед пуском.
	}


	//Начали отсчитывать время, но свет в сети появился.
	if ((ML_step == 1) && (sensors[0] == 0))
	{
		//Сброс флагов не блокирующей задержки
		clear_delay_nolock();
		ML_step = 0;

	}


	if (ML_step == 1) //Выдержка задержки перед пуском.
	{
		//Ключ в положении Off. 
		if (sensors[2] == 1) ML_step = 0;

		if (delay_ms_nolock(setingVal[0] * 1000))
		{
			//Формирует короткий импульс зарытия заслонки,если микроконроллер был включен первый раз damper_flag=0;
			//Таким образом мы удостовериваемся что заслонка действительно закрыта, и не спалим мотор залонки.
			fast_close_dapper();

			//С14 пуск генератора.
			//Перед пуском врернуть заслонку в исходное положение.
			control_dapper(0);//Закрываем заслонку.	Если она не была закрыта(по факту не сработает так как в нутри функции стоит проверка флага заслонки)

			GPIO_SetBits(GPIOA, GPIO_Pin_10);//Подача топлива.
			delay_ms(1000);

			GPIO_SetBits(GPIOC, GPIO_Pin_14); //С14 включение стартера.
			ML_step = 2;
		}
	}

	//Крутим стартер
	if (ML_step == 2)
	{
		//Ключ в положении Off. 
		if (sensors[2] == 1) offAllOut();

		//Выжидаем интервал времени, или если генератор запустился.
		if (delay_ms_nolock(setingVal[2] * 1000) || (sensors[5] == 1)) //Время сколько крутить стартер.
		{
			GPIO_ResetBits(GPIOC, GPIO_Pin_14); //Отключение стартера. С14 управление генератором.
			ML_step = 3;
			attempt_to_run++; //Инкремент количества попыток запуска.
		}
	}

	//Была попытка запустить генератор.
	if (ML_step == 3)
	{
		//Генератор не был запущен.
		if (sensors[5] == 0)
		{
			//Количество попыток запустить генератор, превысило лимит.
			if (attempt_to_run >= setingVal[1])
			{
				ML_step = 250; //Ошибка запуска генератора.
				return;
			}
			else
			{
				if (delay_ms_nolock(setingVal[3] * 1000)) //Время сколько крутить стартер.
				{

					ML_step = 2;//повторная попытка запустить генератор.
					GPIO_SetBits(GPIOC, GPIO_Pin_14); //Повторное включение стартера.
				}
			}

		}
		else //Генератор запущен.
		{
			clear_delay_nolock(); //Сброс флагов не блокирующей задержки.
			ML_step = 4;
		}
	}

	//Генератор уже запущен.
	if (ML_step == 4)
	{
		//Ключ в положении Off. 
		if (sensors[2] == 1) offAllOut();

		//Отсчет времени открытия заслонки.
		if (delay_ms_nolock(setingVal[7] * 1000))
		{
			control_dapper(1);//Открываем заслонку.				
			ML_step = 5; //Заслонка открыта.
		}
	}

	//Заслонка уже открыта.
	if (ML_step == 5)
	{

		//Ключ в положении Off. 
		if (sensors[2] == 1) offAllOut();

		//Отсчет времени подключения нагрузки.
		if (delay_ms_nolock(setingVal[11] * 1000))
		{
			//Включение нагрузки
			control_load_relay(1);
			reset_cooler_soft_timers(); //Сброс таймеров управления вентилятором, иначе вентилятор запуститься быстрее первый раз.
			ML_step = 6;
		}
	}


	//Если генератор заглох? Игнорируем критическую ошибку. Плохо конечно но будем пробовать запустить генератор.
	if ((sensors[5] == 0) && (ML_step > 4) && (ML_step < 200))
	{
		offAllOut();
		ML_step = 0;
	}

	//Напряжение в сети востановилось. Генератор работает.
	if ((sensors[0] == 0) && (ML_step == 6))
	{
		//Ключ в положении Off. 
		if (sensors[2] == 1) offAllOut();

		//Отсчет времени снятия питания на щетках.
		if (delay_ms_nolock(setingVal[12] * 1000))
		{
			//Отключение нагрузки.	
			control_load_relay(0);
			ML_step = 7;
		}
	}


	//Напряжение в сети опять пропало. Генератор работает.
	if ((sensors[0] == 1) && (ML_step == 6))
	{
		//Сброс флагов не блокирующей задержки
		clear_delay_nolock();
	}


	//Нештатная ситуация. Напряжение пропало, а нагрузка отключилась. Но генератор еще работает.
	if ((sensors[0] == 1) && (ML_step == 7))
	{
		//Ключ в положении Off. 
		if (sensors[2] == 1) offAllOut();

		//Подождать время и подключить нагрузку.
		if (delay_ms_nolock(setingVal[0] * 1000))
		{
			control_load_relay(1); //Включение нагрузки.
			ML_step = 6;
		}
	}


	//Напряжение в сети востановилось. Генератор работает. Нагрузка отключена. Выдерживаем интервал закрытия заслонки.
	if ((sensors[0] == 0) && (ML_step == 7))
	{
		//Ключ в положении Off. 
		if (sensors[2] == 1) offAllOut();

		//Отсчет времени закрытия заслонки.
		if (delay_ms_nolock(setingVal[13] * 1000))
		{
			control_dapper(0);//Закрытие заслонки.
			ML_step = 8;
		}
	}


	//Напряжение в сети востановилось. Генератор работает. Нагрузка отключена. Заслонка закрыта.
	if ((sensors[0] == 0) && (ML_step == 8))
	{
		//Ключ в положении Off. 
		if (sensors[2] == 1) offAllOut();

		//Отсчет времени отключения нагрузки.
		if (delay_ms_nolock(setingVal[14] * 1000))
		{
			GPIO_ResetBits(GPIOA, GPIO_Pin_10);//Отключить топливный клапан.

			if (stopRotor() == 1) ML_step = 251; //Передаю ошибку, не смог остановить.
			else
				ML_step = 0; //Остановка двигателя. Успешно.
		}
	}
}

int main(void)
{
	//Установка начального адреса вектора прерываний,для работы ардуиновского бутлоадера.
	NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x2000);
	SysTick_Config(SystemCoreClock / 1000);

	//Отключаю зеленый светодиод PC13, иногда он остается включенный.
	GPIO_InitTypeDef  GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_SetBits(GPIOC, GPIO_Pin_13);

	FLASH_Init();

	init_lcd();
	keyPadInit(); //Инициализация кнопок.	
	init_sensors(); //Инициализация входов, которые являются датчиками.
	hd44780_clear(&lcd);
	hd44780_write_string(&lcd, "Dozor V 1.0 ");
	loadSettings(); //Считывает значения настоек из флеш памяти .

	delay_ms(3000);	//3с

	init_out(); //Инициализая выходов управления реле.


	ML_step = 0; //Сбрасываю шаги основной логики.
	attempt_to_run = 0; //Количество попыток запуска генератора.
	while (1)
	{
		if (wasKeyInterupt == 1)
		{
			//Логика обработки нажатия на кнопки.
			ui_logik();
			wasKeyInterupt = 0;
		}


		ML_Logic(); //"Поток" основной логики.
		//delay_ms(300);		
	}
}

//----------Набор функций необходимых для работы драйвера дисплея.

//const int rs = PB11, en = PB10, d4 = PB6, d5 = PB7, d6 = PB8, d7 = PB9; 
void init_lcd(void)
{
	/* Распиновка дисплея */
	const HD44780_STM32F10x_Pinout lcd_pinout =
	{
	  {
			/* RS        */  { GPIOB, GPIO_Pin_9 }, // GPIOB, GPIO_Pin_11
			/* ENABLE    */  { GPIOB, GPIO_Pin_8 },// GPIOB, GPIO_Pin_10
			/* RW        */  { NULL, 0 },
			/* Backlight */  { NULL, 0 },
			/* DP0       */  { NULL, 0 },
			/* DP1       */  { NULL, 0 },
			/* DP2       */  { NULL, 0 },
			/* DP3       */  { NULL, 0 },
			/* DP4       */  { GPIOB, GPIO_Pin_15 },//GPIO_Pin_6 
			/* DP5       */  { GPIOB, GPIO_Pin_14 },//GPIO_Pin_7
			/* DP6       */  { GPIOB, GPIO_Pin_13 },//GPIO_Pin_8
			/* DP7       */  { GPIOB, GPIO_Pin_12 },//GPIO_Pin_9
		  }
	};

	/* Настраиваем драйвер: указываем интерфейс драйвера (стандартный),
	   указанную выше распиновку и обработчик ошибок GPIO (необязателен). */
	lcd_pindriver.interface = HD44780_STM32F10X_PINDRIVER_INTERFACE;
	/* Если вдруг захотите сами вручную настраивать GPIO для дисплея
	   (зачем бы вдруг), напишите здесь ещё (библиотека учтёт это):
	lcd_pindriver.interface.configure = NULL; */
	lcd_pindriver.pinout = lcd_pinout;
	lcd_pindriver.assert_failure_handler = hd44780_assert_failure_handler;

	/* И, наконец, создаём конфигурацию дисплея: указываем наш драйвер,
	   функцию задержки, обработчик ошибок дисплея (необязателен) и опции.
	   На данный момент доступны две опции - использовать или нет
	   вывод RW дисплея (в последнем случае его нужно прижать к GND),
	   и то же для управления подсветкой. */
	const HD44780_Config lcd_config =
	{
	  (HD44780_GPIO_Interface*)&lcd_pindriver,
	  delay_microseconds,
	  hd44780_assert_failure_handler,
	  HD44780_OPT_NO_USE_RW
	};

	/* Ну, а теперь всё стандартно: подаём тактирование на GPIO,
	   инициализируем дисплей: 16x2, 4-битный интерфейс, символы 5x8 точек. */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	hd44780_init(&lcd, HD44780_MODE_4BIT, &lcd_config, 16, 2, HD44780_CHARSIZE_5x8);
}


void delay_microseconds(uint16_t us)
{
	SysTick->VAL = SysTick->LOAD;
	const uint32_t systick_ms_start = systick_ms;

	while (1)
	{
		uint32_t diff = uint32_time_diff(systick_ms, systick_ms_start);

		if (diff >= ((uint32_t)us / 1000) + (us % 1000 ? 1 : 0))
			break;
	}
}

uint32_t uint32_time_diff(uint32_t now, uint32_t before)
{
	return (now >= before) ? (now - before) : (UINT32_MAX - before + now);
}

void hd44780_assert_failure_handler(const char* filename, unsigned long line)
{
	(void)filename; (void)line;
	do {} while (1);
}

//----------Конец набора функций необходимых для работы драйвера дисплея.
