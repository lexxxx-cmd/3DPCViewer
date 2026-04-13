#include "Controller.h"

Controller::Controller(QObject* parent) : QObject(parent) {
  viewer = std::make_unique<PCViewer>();
  data_service = std::make_unique<DataService>();

  setup_connections();
}

Controller::~Controller() {
}

void Controller::run() {
  viewer->show();
}

void Controller::setup_connections() {
  connect(viewer.get(), &PCViewer::requestProcBag, data_service.get(), &DataService::startProcess);
  connect(viewer.get(), &PCViewer::progressUpdated, data_service.get(), &DataService::updateProgress);

  connect(data_service.get(), &DataService::cloudFrameReady, viewer.get(), &PCViewer::cloudFrameReady);
  connect(data_service.get(), &DataService::imageFrameReady, viewer.get(), &PCViewer::imageFrameReady);
  connect(data_service.get(), &DataService::odomFrameReady, viewer.get(), &PCViewer::odomFrameReady);
  connect(data_service.get(), &DataService::topicListReady, viewer.get(), &PCViewer::topicListReady);
  connect(data_service.get(), &DataService::messageNumReady, viewer.get(), &PCViewer::messageNumReady);
}
