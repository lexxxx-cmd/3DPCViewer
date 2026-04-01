#include "Controller.h"
#include "BagDataTypes.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qRegisterMetaType<LivoxCloudFrame>("LivoxCloudFrame");
    qRegisterMetaType<ImageFrame>("ImageFrame");
    qRegisterMetaType<OdomFrame>("OdomFrame");
    qRegisterMetaType<std::vector<std::string>>("std::vector<std::string>");

    Controller window(nullptr);
    window.run();
    return app.exec();
}
