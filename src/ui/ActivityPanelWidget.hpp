#pragma once

#include "UiModels.hpp"

#include <QFrame>
#include <QVector>

class QListWidget;
class QPushButton;

class ActivityPanelWidget : public QFrame {
    Q_OBJECT

public:
    explicit ActivityPanelWidget(QWidget *parent = nullptr);

    void setItems(const QVector<ActivityItem> &items);

signals:
    void showHistoryRequested();

private:
    QListWidget *m_list = nullptr;
    QPushButton *m_showHistoryButton = nullptr;
};
