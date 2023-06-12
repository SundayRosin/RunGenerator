#include "FirmwareUpdater.h"

FirmwareUpdater::FirmwareUpdater(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
	ui.pushButtonPathToFW->setIcon(QIcon("Image//open-folder-yellow.png"));
	ui.pushButtonBeginFw->setIcon(QIcon("Image//flash.png"));

	connect(ui.pushButtonBeginFw, SIGNAL(pressed()), this, SLOT(slotpushButtonBeginFw()));
	connect(ui.pushButtonPathToFW, SIGNAL(pressed()), this, SLOT(slotpushButtonPathToFW()));
	StrCodec = QTextCodec::codecForName("Windows-1251"); //��������� ���������

	//�������� ������� ����� �������� � ��������. ���� ���� �� ������ ���� � ����.	
	findFw();

	//����� ���������.
	int step = 1;
	if (ui.lineEditPathToFW->text().isEmpty())
	{
		ui.textEditOut->append(StrCodec->toUnicode("1.�������� ���� ��������."));
		step++;
	}
	
	ui.textEditOut->append(QString::number(step) + StrCodec->toUnicode(".���������� ���������� � USB."));
	step++;
	ui.textEditOut->append(QString::number(step) + StrCodec->toUnicode(".������� \"�������\"."));
	step++;
	ui.textEditOut->append(QString::number(step) + StrCodec->toUnicode(".������� \"�����\" �� ����������."));
	step++;
	ui.textEditOut->append(QString::number(step) + StrCodec->toUnicode(".��������� ���������:"));
	step++;
	ui.textEditOut->append( StrCodec->toUnicode("Starting download: [##################################################] finished!\""));
	ui.textEditOut->append( StrCodec->toUnicode("state(8) = dfuMANIFEST - WAIT - RESET, status(0) = No error condition is present"));
	ui.textEditOut->append( StrCodec->toUnicode("Done!"));
	ui.textEditOut->append(QString::number(step) + StrCodec->toUnicode(".��������� 5 ������."));
	step++;
	ui.textEditOut->append(QString::number(step) + StrCodec->toUnicode(".������������� ����������."));

	//���� ��������� ����������� � �������� Keil,���� ������� Objects, 
	//���������� �������������� ��������������� �����.
	//runInKeilDir();

	int y = 0;
	
}

//�������� ������� ����� �������� � ��������. ���� ���� �� ������ ���� � ����.	
void FirmwareUpdater::findFw()
{
	//������������� ���� � ��������.
	QString path = QDir::currentPath() + "//Fw"; //���� � ��������.

	 //���� �� �������?
	QDir dir(path);
	if (!dir.exists()) return; //�������, ��� ��� �� ���� ������.

   //����� ������ � ��������.
	foreach(QFileInfo item, dir.entryInfoList())
	{
		if (item.isFile())
		{
			if (item.suffix() == "bin")
			{
				ui.lineEditPathToFW->setText(item.absoluteFilePath());
				ui.textEditOut->append(
					StrCodec->toUnicode("�������� ����� ����� �� �������� Fw."));
				break;
			}
			
		}
	}

}


//����������� � �������� ����������  Keil.
void FirmwareUpdater::runInKeilDir()
{
	//������������� ���� � ��������.
//	QString path = QDir::currentPath() + "//Objects"; //���� � ��������.
	QString path = "..//Objects"; //���� � ��������.

												 //���� �� �������?
	QDir dir(path);
	if (!dir.exists()) return; //�������, ��� ��� �� ���� ������

	ui.textEditOut->append(
		StrCodec->toUnicode("������ � ���������� Keil."));

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
   QString fileName = QFileDialog::getOpenFileName(this, StrCodec->toUnicode("����� ����� ��������."),
		"", StrCodec->toUnicode("����� �������� (*.bin)"));
	ui.lineEditPathToFW->setText(fileName);
			
}

void FirmwareUpdater::slotpushButtonBeginFw()
{
	ui.textEditOut->clear();//������� ���� ���� ��������� ���.

	if (!checkJavaVerion()) return; //�������� ������ java.

	//������� �������� ������������ ����� �����
	QString path = ui.lineEditPathToFW->text();
	if (path.isEmpty())
	{
		QMessageBox::warning(this, StrCodec->toUnicode("��������"), StrCodec->toUnicode("�� ������ ���� � ����� ��������."));
		return;
	}

	try
	{
		QString dir_name = "Tmp"; //��� ���������� ��������.

		//������� ������� ���� ���.
		QDir dir("Tmp");
		if (!dir.exists()) {
			dir.mkpath(".");
		}

		//������� ��� �����.
		QFileInfo file(path);
		QString name = file.fileName();


		//If destination file exist, QFile::copy will not work.The solution is to verify if destination file exist, then delete it:
		if (QFile::exists(".//Tmp"+name))
		{
			QFile::remove(".//Tmp"+name);
		}

		QFile::copy(path, ".//Tmp//"+name); //������� ����.
		ui.textEditOut->append(StrCodec->toUnicode("���� �������� ���������� �� ��������� �����."));
		
		runFw(name); //������ �������� ��������.

	}
	catch (...)
	{
		QMessageBox::critical(this, StrCodec->toUnicode("��������"), 
			StrCodec->toUnicode("��� ������� ����������� ���� �������� � ������� Fw �������� ����������."));
		return;
	}


}

//��������� ����������� �� java �� ����������.
bool FirmwareUpdater::checkJavaVerion()
{	
	QProcess process;
	/*
	 ��������� �������� ������. ��������� ��������� ������������ �� ������ ������.
	 ��������� ���������� ����� ������ ��� ������ �������� ������� �������, ��������� ���������� 
	 "�����������" �����, ��������� ���. ����� ���������� ��.
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

	//�������� �� �����.
	if (!output.contains("java version"))
	{
		QMessageBox::critical(this, StrCodec->toUnicode("��������"),
			StrCodec->toUnicode("������ �� ���������� �� ����������� java. ��� �������� �������� ���������:") +
			output);
		return false;
	}

	//��� ������������� ��������� � ������ java.
	int pos = output.indexOf('\n');

	QString str1 = output.left(pos);
	str1 = str1.replace("java version","");
	str1 = str1.replace('\"', ' ');
	str1 = str1.replace(" ", "");

	ui.textEditOut->append(StrCodec->toUnicode("������ Java:")+str1);

	return 1;//���� ���.

}

//��������� ������� ��������.
void FirmwareUpdater::runFw(QString filename)
{
	//��� ���� �� ����� ��� ��� �� �� ������������. ����� ������ �����.
	QString exe_path = QDir::currentPath();
	QString path_maple = ".//maple_loader//maple_upload.bat"; //������.
	QString path_fw = exe_path+"//Tmp//"+filename;

	/*
	���������
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