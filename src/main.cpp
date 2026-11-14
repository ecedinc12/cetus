#include "ui/MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("cetus"));
    QApplication::setOrganizationName(QStringLiteral("cetus"));

    MainWindow window;
    window.show();
    return app.exec();
}
