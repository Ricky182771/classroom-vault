#include "KpiCardWidget.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

KpiCardWidget::KpiCardWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("KpiCard"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(6);

    auto *topRow = new QHBoxLayout();
    m_iconLabel = new QLabel(QStringLiteral("•"), this);
    m_iconLabel->setProperty("subtle", true);
    m_label = new QLabel(QStringLiteral("KPI"), this);
    m_label->setProperty("muted", true);

    topRow->addWidget(m_iconLabel);
    topRow->addWidget(m_label);
    topRow->addStretch(1);
    layout->addLayout(topRow);

    m_value = new QLabel(QStringLiteral("0"), this);
    m_value->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: 700; background: transparent; border: none;"));
    layout->addWidget(m_value);
}

void KpiCardWidget::setData(const KpiData &data)
{
    m_iconLabel->setText(data.iconName.isEmpty() ? QStringLiteral("•") : data.iconName);
    m_label->setText(data.label.isEmpty() ? QStringLiteral("KPI") : data.label);
    m_value->setText(QString::number(data.value));

    QString status = data.status.trimmed();
    if (status.isEmpty()) {
        status = QStringLiteral("idle");
    }
    m_value->setProperty("status", status);

    QString color = QStringLiteral("#9AA0AE");
    if (status == QStringLiteral("complete")) {
        color = QStringLiteral("#8FD19E");
    } else if (status == QStringLiteral("pending")) {
        color = QStringLiteral("#E6C26A");
    } else if (status == QStringLiteral("error")) {
        color = QStringLiteral("#E07C7C");
    }

    m_value->setStyleSheet(
        QStringLiteral("font-size: 24px; font-weight: 700; color: %1; background: transparent; border: none;").arg(color));
}
