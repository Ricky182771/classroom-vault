#pragma once

#include "UiModels.hpp"

#include <QFrame>

class QLabel;

class KpiCardWidget : public QFrame {
    Q_OBJECT

public:
    explicit KpiCardWidget(QWidget *parent = nullptr);

    void setData(const KpiData &data);

private:
    QLabel *m_iconLabel = nullptr;
    QLabel *m_label = nullptr;
    QLabel *m_value = nullptr;
};
