#include "FirmwareUpdater.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
	QIcon icon("Image/icon.png");

    FirmwareUpdater w;
	w.setWindowIcon(icon);
    w.show();
    return a.exec();

}
