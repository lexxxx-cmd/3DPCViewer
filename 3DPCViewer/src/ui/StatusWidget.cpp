#include "ui/StatusWidget.h"

StatusWidget::StatusWidget(QWidget *parent)
	: QWidget(parent), topicModel(nullptr)
{
	ui = std::make_unique< Ui::StatusWidgetClass>();
	ui->setupUi(this);
}

StatusWidget::~StatusWidget() = default;

void StatusWidget::onUpdateTopicList(const std::vector<std::string>& topics) {
    // 1. ��ʼ�������ģ��
    if (!topicModel) {
        topicModel = new QStandardItemModel(this);
        topicModel->setHorizontalHeaderLabels({ "Topic Name" });

        // ���źţ��� Item �Ĺ�ѡ״̬�仯ʱ����
        connect(topicModel, &QStandardItemModel::itemChanged,
            this, &StatusWidget::onTopicStateChanged);
    }
    else {
        topicModel->clear();
        topicModel->setHorizontalHeaderLabels({ "Topic Name" });
    }

    // 2. �����źţ���ֹ�����Ӵ�������ʱƵ������ itemChanged �ۺ�����
    topicModel->blockSignals(true);

    // 3. �������
    for (const auto& topic : topics) {
        QStandardItem* item = new QStandardItem(QString::fromStdString(topic));

        // ���Ĳ�������Ϊ��ѡ�������ʼ״̬
        item->setCheckable(true);
        item->setCheckState(Qt::Unchecked); // Ĭ�ϲ���ѡ
        item->setEditable(false);           // ��ֹ˫���޸�����

        topicModel->appendRow(item);
    }

    // 4. �ָ��źŲ�������ͼ
    topicModel->blockSignals(false);
    ui->treeView->setModel(topicModel);
}

void StatusWidget::onTopicStateChanged(QStandardItem* item) {
    if (!item) return;

    QString topicName = item->text();
    bool isChecked = (item->checkState() == Qt::Checked);

    emit topicSelected(topicName, isChecked);
}


