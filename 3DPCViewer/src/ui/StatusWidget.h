#pragma once

#include <QWidget>
#include <memory>
#include <QStandardItemModel>
#include <QHash>
#include <vector>
#include <string>
#include "ui_StatusWidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class StatusWidgetClass; };
QT_END_NAMESPACE

class StatusWidget : public QWidget
{
	Q_OBJECT

public:
	StatusWidget(QWidget *parent = nullptr);
	~StatusWidget();

signals:
	void bagNodeActivated(int bagIndex);
	void topicSelectionChanged(int bagIndex, std::vector<std::string> checkedRawTopics);

public slots:
	void onUpdateTopicList(const std::vector<std::string>& topics);
	void onTopicStateChanged(QStandardItem* item);
	void onTreeItemClicked(const QModelIndex& index);

private:
	enum NodeKind {
		ParentNode = 0,
		ChildNode = 1
	};

	enum ItemRole {
		NodeKindRole = Qt::UserRole + 1,
		BagIndexRole,
		RawTopicNameRole,
		PrefixedTopicNameRole
	};

	static int parseBagIndex(const QString& prefixedTopicName);
	static QString extractRawTopicName(const QString& prefixedTopicName);
	QStandardItem* ensureBagParentItem(int bagIndex);
	void resetBagChildrenUnchecked(int bagIndex);
	std::vector<std::string> collectCheckedRawTopics(int bagIndex) const;

	std::unique_ptr<Ui::StatusWidgetClass> ui;
	QStandardItemModel* topicModel;
	QHash<int, QStandardItem*> m_bagParentItems;
	int m_activeBagIndex{0};
	bool m_ignoreItemChanged{false};
};
