<h2 align="center">Прошивка отладочной платы.</h2>
<p>
Для прошивки потребуется преобразователь USB-UART. Подойдет любой. На рисунке плата преобразователя на основе чипа CP2102. Необходимо установить драйвера на вашу модель, и проверить определяется ли устройство.<br>
<img src="./Images/UartPort.jpg" width="40%">
</p>
<p>
Далее припаиваем провода от преобразователя к отладочной плате. Потребуется 4 провода.<br>
Преобразователь TX - отладочная плата A10<br>
Преобразователь RX - отладочная плата A9<br>
Питание «плюс» и «минус».<br>
Стоит иметь ввиду – если неправильно согласованы уровни сигналов (вместо 3.3в приходит сигнал в 5 вольт) – плата может выйти из строя. К этому может привести и короткое замыкание сигнальных проводников. Обязательно проверяйте качество пайки.<br>
<img src="./Images/board1.jpg" width="40%">
</p>
<p>
Установкой перемычек, «включаем» работу загрузчика микроконтроллера.<br>
<img src="./Images/board2.jpg" width="40%">
</p>
<p>
Для загрузки прошивки необходимо скачать (или взять в .\Binaries\FlashLoader.zip) программу Flash Loader Demonstrator.<br>
Запускаем программу. Устанавливаем порт, на котором определился переходник.<br>
<img src="./Images/Loader1.jpg" width="40%"><br>
Выбираем семейство<br>
<img src="./Images/Loader2.jpg" width="40%"><br>
<br>
Для данного проекта используем загрузчик Arduino.
Для прошивки загрузчика необходимо изменить тип файлов на *.bin<br>
<img src="./Images/Loader3.jpg" width="40%"><br>
Прошиваем.<br>
<img src="./Images/Loader4.jpg" width="40%"><br>
Возвращаем перемычки в исходное положение.
</p>
<p>
<a href="../HowLoadFirmware.md">После прошивки загрузчика, необходимо настроить среду разработки.</a>
</p>