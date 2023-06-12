#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_FirmwareUpdater.h"
#include <QFileDialog>
#include <QThread>
#include <QFileinfo>
#include <QTextCodec>
#include <QMessageBox>
#include <QProcess>
#include <qstring.h>
#include <QDebug>
#include <QSettings>

class FirmwareUpdater : public QMainWindow
{
    Q_OBJECT

public:
    FirmwareUpdater(QWidget *parent = Q_NULLPTR);

private:
    Ui::FirmwareUpdaterClass ui;
	QTextCodec *StrCodec;

	bool FirmwareUpdater::checkJavaVerion();
	QProcess processUploadFw;
	
	//Проверка наличия файла прошивки в каталоге. Если есть то ставит путь к нему.	
	void findFw();

	void getPath();
	//Запускается в корневой директории  Keil.
	void FirmwareUpdater::runInKeilDir(); 


	//Запускает процесс прошивки.
	void FirmwareUpdater::runFw(QString filename);


	private slots:
	void slotpushButtonBeginFw();//Нажатие на кнопку зашифровать
	void slotpushButtonPathToFW();//Нажатие на кнопку расшифровать
								  //Проверяет установлена ли java на компьютере.
	void updateText();



};
