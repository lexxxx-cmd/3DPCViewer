#include "pcviewer.h"
#include "BagDataTypes.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qRegisterMetaType<LivoxCloudFrame>("LivoxCloudFrame");
    qRegisterMetaType<ImageFrame>("ImageFrame");
    qRegisterMetaType<OdomFrame>("OdomFrame");

    PCViewer window;
    window.show();
    return app.exec();
}
