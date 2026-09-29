#include "mainwindow.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("NetDisk Qt Client"));
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    MainWindow window;
    window.show();
    return application.exec();
}
