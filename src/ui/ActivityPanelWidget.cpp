#include "ActivityPanelWidget.hpp"

#include <QHBoxLayout>
#include <QColor>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

ActivityPanelWidget::ActivityPanelWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("ActivityPanel"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *titleRow = new QHBoxLayout();
    auto *title = new QLabel(QStringLiteral("Actividad reciente"), this);
    title->setStyleSheet(QStringLiteral("font-weight: 700;"));
    titleRow->addWidget(title);
    titleRow->addStretch(1);
    layout->addLayout(titleRow);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setAlternatingRowColors(true);
    layout->addWidget(m_list, 1);

    m_showHistoryButton = new QPushButton(QStringLiteral("Ver historial completo"), this);
    m_showHistoryButton->setProperty("variant", QStringLiteral("ghost"));
    layout->addWidget(m_showHistoryButton);

    connect(m_showHistoryButton, &QPushButton::clicked, this, &ActivityPanelWidget::showHistoryRequested);
}

void ActivityPanelWidget::setItems(const QVector<ActivityItem> &items)
{
    m_list->clear();

    for (const ActivityItem &item : items) {
        const QString line = QStringLiteral("[%1] %2  %3")
                                 .arg(item.time.isEmpty() ? QStringLiteral("--:--:--") : item.time)
                                 .arg(item.kind.isEmpty() ? QStringLiteral("INFO") : item.kind)
                                 .arg(item.message);
        auto *listItem = new QListWidgetItem(line, m_list);

        const QString kind = item.kind.toUpper();
        if (kind == QStringLiteral("ERROR") || kind == QStringLiteral("ERR")) {
            listItem->setForeground(QColor(QStringLiteral("#E07C7C")));
        } else if (kind == QStringLiteral("SYNC")) {
            listItem->setForeground(QColor(QStringLiteral("#8EA2FF")));
        } else if (kind == QStringLiteral("FILE")) {
            listItem->setForeground(QColor(QStringLiteral("#8FD19E")));
        } else if (kind == QStringLiteral("EXPORT")) {
            listItem->setForeground(QColor(QStringLiteral("#86C8E8")));
        } else if (kind == QStringLiteral("LINK")) {
            listItem->setForeground(QColor(QStringLiteral("#C7A6FF")));
        } else if (kind == QStringLiteral("SKIP")) {
            listItem->setForeground(QColor(QStringLiteral("#A3A8B5")));
        }
    }
}
