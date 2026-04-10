#include "ui/StatusWidget.h"

StatusWidget::StatusWidget(QWidget *parent)
	: QWidget(parent), topicModel(nullptr)
{
	ui = std::make_unique< Ui::StatusWidgetClass>();
	ui->setupUi(this);
    connect(ui->treeView, &QTreeView::clicked,
        this, &StatusWidget::onTreeItemClicked);
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
    bagItem->setSelectable(true);
    bagItem->setData(ParentNode, NodeKindRole);
    bagItem->setData(bagIndex, BagIndexRole);
    bagItem->setData(QString("/bag%1").arg(bagIndex), PrefixedTopicNameRole);
    topicModel->appendRow(bagItem);
    m_bagParentItems.insert(bagIndex, bagItem);
    return bagItem;
}

void StatusWidget::resetBagChildrenUnchecked(int bagIndex)
{
    if (!m_bagParentItems.contains(bagIndex)) {
        return;
    }
    QStandardItem* bagItem = m_bagParentItems.value(bagIndex);

    m_ignoreItemChanged = true;
    for (int i = 0; i < bagItem->rowCount(); ++i) {
        QStandardItem* childItem = bagItem->child(i);
        if (childItem && childItem->checkState() != Qt::Unchecked) {
            childItem->setCheckState(Qt::Unchecked);
        }
    }
    m_ignoreItemChanged = false;
}

std::vector<std::string> StatusWidget::collectCheckedRawTopics(int bagIndex) const
{
    std::vector<std::string> checkedTopics;
    if (!m_bagParentItems.contains(bagIndex)) {
        return checkedTopics;
    }

    const QStandardItem* bagItem = m_bagParentItems.value(bagIndex);
    for (int i = 0; i < bagItem->rowCount(); ++i) {
        const QStandardItem* childItem = bagItem->child(i);
        if (!childItem || childItem->checkState() != Qt::Checked) {
            continue;
        }
        const QString rawTopic = childItem->data(RawTopicNameRole).toString();
        if (!rawTopic.isEmpty()) {
            checkedTopics.emplace_back(rawTopic.toStdString());
        }
    }
    return checkedTopics;
}

void StatusWidget::onUpdateTopicList(const std::vector<std::string>& topics) {
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

    const QString firstTopic = QString::fromStdString(topics.front());
    const int bagIndex = parseBagIndex(firstTopic);
    if (bagIndex <= 0) {
        return;
    }

    QStandardItem* bagItem = ensureBagParentItem(bagIndex);
    bagItem->removeRows(0, bagItem->rowCount());

    for (const auto& topic : topics) {
        const QString prefixedTopicName = QString::fromStdString(topic);
        const int topicBagIndex = parseBagIndex(prefixedTopicName);
        if (topicBagIndex != bagIndex) {
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
        item->setData(ChildNode, NodeKindRole);
        item->setData(bagIndex, BagIndexRole);
        item->setData(rawTopicName, RawTopicNameRole);
        item->setData(prefixedTopicName, PrefixedTopicNameRole);
        bagItem->appendRow(item);
    }
    ui->treeView->expand(bagItem->index());
}

void StatusWidget::onTopicStateChanged(QStandardItem* item) {
    if (!item || m_ignoreItemChanged) {
        return;
    }

    const int nodeKind = item->data(NodeKindRole).toInt();
    if (nodeKind != ChildNode) {
        return;
    }

    const int bagIndex = item->data(BagIndexRole).toInt();
    if (bagIndex <= 0 || bagIndex != m_activeBagIndex) {
        return;
    }

    emit topicSelectionChanged(bagIndex, collectCheckedRawTopics(bagIndex));
}

void StatusWidget::onTreeItemClicked(const QModelIndex& index)
{
    if (!index.isValid() || !topicModel) {
        return;
    }

    QStandardItem* item = topicModel->itemFromIndex(index);
    if (!item) {
        return;
    }

    const int nodeKind = item->data(NodeKindRole).toInt();
    const int bagIndex = item->data(BagIndexRole).toInt();
    if (bagIndex <= 0) {
        return;
    }

    if (nodeKind == ParentNode) {
        if (bagIndex != m_activeBagIndex) {
            m_activeBagIndex = bagIndex;
            resetBagChildrenUnchecked(bagIndex);
            emit topicSelectionChanged(bagIndex, {});
        }
        emit bagNodeActivated(bagIndex);
    }
}
