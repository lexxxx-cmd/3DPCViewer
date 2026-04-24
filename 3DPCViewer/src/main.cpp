#include "core/Controller.h"
#include "io/BagDataTypes.h"
#include "slam/SlamNetProtocol.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qRegisterMetaType<GeneralCloudFrame>("GeneralCloudFrame");
    qRegisterMetaType<ImageFrame>("ImageFrame");
    qRegisterMetaType<OdomFrame>("OdomFrame");
    qRegisterMetaType<std::vector<std::string>>("std::vector<std::string>");
    qRegisterMetaType<slam::net::Command>("slam::net::Command");
    qRegisterMetaType<QList<QByteArray>>("QList<QByteArray>");

    Controller window(nullptr);
    window.run();
    return app.exec();
}
