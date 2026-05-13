#pragma once

#include "UiModels.hpp"

#include <QFrame>
#include <QVector>

class QListWidget;
class QPushButton;
class QWidget;

class ActivityDrawerWidget : public QFrame {
    Q_OBJECT

public:
    explicit ActivityDrawerWidget(QWidget *parent = nullptr);

    void setItems(const QVector<ActivityItem> &items);

signals:
    void clearRequested();

private:
    bool m_expanded = false;

    QPushButton *m_toggleButton = nullptr;
    QPushButton *m_clearButton = nullptr;
    QWidget *m_content = nullptr;
    QListWidget *m_list = nullptr;
};
