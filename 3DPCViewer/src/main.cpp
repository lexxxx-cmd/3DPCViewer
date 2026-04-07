#include "core/Controller.h"
#include "io/BagDataTypes.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qRegisterMetaType<GeneralCloudFrame>("GeneralCloudFrame");
    qRegisterMetaType<ImageFrame>("ImageFrame");
    qRegisterMetaType<OdomFrame>("OdomFrame");
    qRegisterMetaType<std::vector<std::string>>("std::vector<std::string>");

    Controller window(nullptr);
    window.run();
    return app.exec();
}
