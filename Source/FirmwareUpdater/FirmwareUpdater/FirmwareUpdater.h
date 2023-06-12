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
	
	//�������� ������� ����� �������� � ��������. ���� ���� �� ������ ���� � ����.	
	void findFw();

	void getPath();
	//����������� � �������� ����������  Keil.
	void FirmwareUpdater::runInKeilDir(); 


	//��������� ������� ��������.
	void FirmwareUpdater::runFw(QString filename);


	private slots:
	void slotpushButtonBeginFw();//������� �� ������ �����������
	void slotpushButtonPathToFW();//������� �� ������ ������������
								  //��������� ����������� �� java �� ����������.
	void updateText();



};
