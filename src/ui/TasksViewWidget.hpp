#pragma once

#include "UiModels.hpp"

#include <QFrame>
#include <QVector>

class QLabel;
class QTableWidget;

class TasksViewWidget : public QFrame {
    Q_OBJECT

public:
    explicit TasksViewWidget(QWidget *parent = nullptr);

    void setRows(const QVector<TaskRowData> &rows);

signals:
    void taskActivated(const QString &courseId, const QString &assignmentId);

private:
    QLabel *m_title = nullptr;
    QTableWidget *m_table = nullptr;
};
