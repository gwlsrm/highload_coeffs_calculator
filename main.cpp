#include "mainwindow.h"

#include <QApplication>
#include <QIcon>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication a(argc, argv);
    QApplication::setWindowIcon(QIcon(":/icons/app.png"));

    QTranslator translator;
    if (translator.load("app_ru",
                        QCoreApplication::applicationDirPath()
                            + "/translations")) {
        a.installTranslator(&translator);
    }

    MainWindow w;
    w.show();
    return a.exec();
}
