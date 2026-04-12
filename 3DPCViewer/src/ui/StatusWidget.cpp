#include "ui/StatusWidget.h"

StatusWidget::StatusWidget(QWidget *parent)
    : QWidget(parent), topicModel(nullptr)
{
    ui = std::make_unique<Ui::StatusWidgetClass>();
    ui->setupUi(this);
}

StatusWidget::~StatusWidget() = default;

int StatusWidget::parseBagIndex(const QString& prefixedTopicName)
{
    if (!prefixedTopicName.startsWith("/bag")) {
        return 0;
    }
    const int slashPos = prefixedTopicName.indexOf('/', 4);
    const QString bagPart = (slashPos < 0)
        ? prefixedTopicName.mid(4)
        : prefixedTopicName.mid(4, slashPos - 4);
    bool ok = false;
    const int bagIndex = bagPart.toInt(&ok);
    return ok ? bagIndex : 0;
}

QString StatusWidget::extractRawTopicName(const QString& prefixedTopicName)
{
    const int slashPos = prefixedTopicName.indexOf('/', 4);
    if (slashPos < 0) {
        return QString();
    }
    return prefixedTopicName.mid(slashPos);
}

QStandardItem* StatusWidget::ensureBagParentItem(int bagIndex)
{
    if (m_bagParentItems.contains(bagIndex)) {
        return m_bagParentItems.value(bagIndex);
    }

    QStandardItem* bagItem = new QStandardItem(QString("/bag%1").arg(bagIndex));
    bagItem->setEditable(false);
    topicModel->appendRow(bagItem);
    m_bagParentItems.insert(bagIndex, bagItem);
    return bagItem;
}

void StatusWidget::onUpdateTopicList(const std::vector<std::string>& topics)
{
    if (topics.empty()) {
        return;
    }

    if (!topicModel) {
        topicModel = new QStandardItemModel(this);
        topicModel->setHorizontalHeaderLabels({ "Topic Name" });
        connect(topicModel, &QStandardItemModel::itemChanged,
            this, &StatusWidget::onTopicStateChanged);
        ui->treeView->setModel(topicModel);
    }

    const int bagIndex = parseBagIndex(QString::fromStdString(topics.front()));
    if (bagIndex <= 0) {
        return;
    }

    QStandardItem* bagItem = ensureBagParentItem(bagIndex);
    bagItem->removeRows(0, bagItem->rowCount());

    for (const auto& topic : topics) {
        const QString prefixedTopicName = QString::fromStdString(topic);
        if (parseBagIndex(prefixedTopicName) != bagIndex) {
            continue;
        }

        const QString rawTopicName = extractRawTopicName(prefixedTopicName);
        if (rawTopicName.isEmpty()) {
            continue;
        }

        QStandardItem* item = new QStandardItem(rawTopicName);
        item->setCheckable(true);
        item->setCheckState(Qt::Unchecked);
        item->setEditable(false);
        bagItem->appendRow(item);
    }

    ui->treeView->expand(bagItem->index());
}

void StatusWidget::onTopicStateChanged(QStandardItem* item)
{
    Q_UNUSED(item);
}
