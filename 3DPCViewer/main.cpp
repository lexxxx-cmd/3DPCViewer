#include "pcviewer.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    PCViewer window;
    window.show();
    return app.exec();
}
