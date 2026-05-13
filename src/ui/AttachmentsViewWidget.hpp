#pragma once

#include "UiModels.hpp"

#include <QFrame>
#include <QVector>

class QLabel;
class QTableWidget;

class AttachmentsViewWidget : public QFrame {
    Q_OBJECT

public:
    explicit AttachmentsViewWidget(QWidget *parent = nullptr);

    void setRows(const QVector<AttachmentRowData> &rows);

private:
    QLabel *m_title = nullptr;
    QTableWidget *m_table = nullptr;
};
