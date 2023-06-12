#include "FirmwareUpdater.h"

FirmwareUpdater::FirmwareUpdater(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
	ui.pushButtonPathToFW->setIcon(QIcon("Image//open-folder-yellow.png"));
	ui.pushButtonBeginFw->setIcon(QIcon("Image//flash.png"));

	connect(ui.pushButtonBeginFw, SIGNAL(pressed()), this, SLOT(slotpushButtonBeginFw()));
	connect(ui.pushButtonPathToFW, SIGNAL(pressed()), this, SLOT(slotpushButtonPathToFW()));
	StrCodec = QTextCodec::codecForName("Windows-1251"); //Установка кодировки

	//Проверка наличия файла прошивки в каталоге. Если есть то ставит путь к нему.	
	findFw();

	//Вывод подсказки.
	int step = 1;
	if (ui.lineEditPathToFW->text().isEmpty())
	{
		ui.textEditOut->append(StrCodec->toUnicode("1.Выберите файл прошивки."));
		step++;
	}
	
	ui.textEditOut->append(QString::number(step) + StrCodec->toUnicode(".Подключите устройство к USB."));
	step++;
	ui.textEditOut->append(QString::number(step) + StrCodec->toUnicode(".Нажмите \"Прошить\"."));
	step++;
	ui.textEditOut->append(QString::number(step) + StrCodec->toUnicode(".Нажмите \"Сброс\" на устройстве."));
	step++;
	ui.textEditOut->append(QString::number(step) + StrCodec->toUnicode(".Дождитесь сообщения:"));
	step++;
	ui.textEditOut->append( StrCodec->toUnicode("Starting download: [##################################################] finished!\""));
	ui.textEditOut->append( StrCodec->toUnicode("state(8) = dfuMANIFEST - WAIT - RESET, status(0) = No error condition is present"));
	ui.textEditOut->append( StrCodec->toUnicode("Done!"));
	ui.textEditOut->append(QString::number(step) + StrCodec->toUnicode(".Подождите 5 секунд."));
	step++;
	ui.textEditOut->append(QString::number(step) + StrCodec->toUnicode(".Перезагрузите устройство."));

	//Если программа запускается в каталоге Keil,есть каталог Objects, 
	//производим автоматическое конвертирование файла.
	//runInKeilDir();

	int y = 0;
	
}

//Проверка наличия файла прошивки в каталоге. Если есть то ставит путь к нему.	
void FirmwareUpdater::findFw()
{
	//Относительный путь к каталогу.
	QString path = QDir::currentPath() + "//Fw"; //Путь к каталогу.

	 //Есть ли каталог?
	QDir dir(path);
	if (!dir.exists()) return; //Выходим, так как не чего искать.

   //Поиск файлов в каталоге.
	foreach(QFileInfo item, dir.entryInfoList())
	{
		if (item.isFile())
		{
			if (item.suffix() == "bin")
			{
				ui.lineEditPathToFW->setText(item.absoluteFilePath());
				ui.textEditOut->append(
					StrCodec->toUnicode("Прошивка будет взята из каталога Fw."));
				break;
			}
			
		}
	}

}


//Запускается в корневой директории  Keil.
void FirmwareUpdater::runInKeilDir()
{
	//Относительный путь к каталогу.
//	QString path = QDir::currentPath() + "//Objects"; //Путь к каталогу.
	QString path = "..//Objects"; //Путь к каталогу.

												 //Есть ли каталог?
	QDir dir(path);
	if (!dir.exists()) return; //Выходим, так как не чего искать

	ui.textEditOut->append(
		StrCodec->toUnicode("Запуск в директории Keil."));

}


void FirmwareUpdater::getPath()
{

	QSettings settings(QLatin1String("HKEY_CURRENT_USER\SOFTWARE"), QSettings::NativeFormat);
	qDebug() << settings.fileName();

	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	QString output= env.value("path");
	
	QStringList listPaths = output.split(';');
	

	QString filename = "Data.txt";
	QFile file(filename);
	if (file.open(QIODevice::ReadWrite)) {
		
	}

	foreach(const QString &str, listPaths) {
		QString f = str;
		if(f.contains("Keil"))
		qDebug("%s", qUtf8Printable(f));

		QTextStream stream(&file);
		stream << f << endl;
	}

}



void FirmwareUpdater::slotpushButtonPathToFW()
{
   QString fileName = QFileDialog::getOpenFileName(this, StrCodec->toUnicode("Выбор файла прошивки."),
		"", StrCodec->toUnicode("Файлы прошивки (*.bin)"));
	ui.lineEditPathToFW->setText(fileName);
			
}

void FirmwareUpdater::slotpushButtonBeginFw()
{
	ui.textEditOut->clear();//Очистка если шьем несколько раз.

	if (!checkJavaVerion()) return; //Проверка версии java.

	//Сделать проверку правильности файла ключа
	QString path = ui.lineEditPathToFW->text();
	if (path.isEmpty())
	{
		QMessageBox::warning(this, StrCodec->toUnicode("Внимание"), StrCodec->toUnicode("Не выбран путь к файлу прошивки."));
		return;
	}

	try
	{
		QString dir_name = "Tmp"; //Имя временного каталога.

		//Создаем каталог если нет.
		QDir dir("Tmp");
		if (!dir.exists()) {
			dir.mkpath(".");
		}

		//Получаю имя файла.
		QFileInfo file(path);
		QString name = file.fileName();


		//If destination file exist, QFile::copy will not work.The solution is to verify if destination file exist, then delete it:
		if (QFile::exists(".//Tmp"+name))
		{
			QFile::remove(".//Tmp"+name);
		}

		QFile::copy(path, ".//Tmp//"+name); //Копирую файл.
		ui.textEditOut->append(StrCodec->toUnicode("Файл прошивки скопирован во временную папку."));
		
		runFw(name); //Запуск процесса прошивки.

	}
	catch (...)
	{
		QMessageBox::critical(this, StrCodec->toUnicode("Внимание"), 
			StrCodec->toUnicode("При попытке скопировать файл прошивки в каталог Fw возникло исключение."));
		return;
	}


}

//Проверяет установлена ли java на компьютере.
bool FirmwareUpdater::checkJavaVerion()
{	
	QProcess process;
	/*
	 объединит выходные каналы. Различные программы записываются на разные выходы.
	 Некоторые используют вывод ошибки для своего обычного ведения журнала, некоторые используют 
	 "стандартный" вывод, некоторые оба. Лучше объединить их.
	*/
	process.setProcessChannelMode(QProcess::MergedChannels);
	process.start("java -version");


	// Get the output
		QString output;
	if (process.waitForStarted(-1)) {
		while (process.waitForReadyRead(-1)) {
			output += process.readAll();
		}
	}
	process.waitForFinished();

	//Содержит ли текст.
	if (!output.contains("java version"))
	{
		QMessageBox::critical(this, StrCodec->toUnicode("Внимание"),
			StrCodec->toUnicode("Похоже на компьютере не установлена java. При проверку получено сообщение:") +
			output);
		return false;
	}

	//Где заканчивается сообщение о версии java.
	int pos = output.indexOf('\n');

	QString str1 = output.left(pos);
	str1 = str1.replace("java version","");
	str1 = str1.replace('\"', ' ');
	str1 = str1.replace(" ", "");

	ui.textEditOut->append(StrCodec->toUnicode("Версия Java:")+str1);

	return 1;//Есть ява.

}

//Запускает процесс прошивки.
void FirmwareUpdater::runFw(QString filename)
{
	//Ком порт не нужен так как он не используется. Можно задать любой.
	QString exe_path = QDir::currentPath();
	QString path_maple = ".//maple_loader//maple_upload.bat"; //Батник.
	QString path_fw = exe_path+"//Tmp//"+filename;

	/*
	Параметры
	java -jar maple_loader.jar %1 %2 %3 %4 %5 %6 %7 %8 %9
	  String comPort = args[0];   //
        String altIf = args[1];     // ?
        String usbID = args[2];     // "1EAF:0003";
        String binFile = args[3];   // bin file
	*/

	QString arg = path_maple+" COM7 2 1EAF:0003 "+"\""+ path_fw +"\"";
	

	processUploadFw.setProcessChannelMode(QProcess::MergedChannels);
	connect(&processUploadFw, SIGNAL(readyRead()), this, SLOT(updateText()));
	processUploadFw.start(arg,  QIODevice::ReadWrite);

	
	// Wait for it to start
	if (!processUploadFw.waitForStarted())
		return ;	

	processUploadFw.waitForFinished();
}

void FirmwareUpdater::updateText()
{
	QString appendText= StrCodec->toUnicode( processUploadFw.readAll());
	ui.textEditOut->append(appendText);
	QCoreApplication::processEvents();
}