#include "ActivityDrawerWidget.hpp"

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

ActivityDrawerWidget::ActivityDrawerWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("ActivityDrawer"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 4, 8, 4);
    root->setSpacing(6);

    auto *bar = new QHBoxLayout();
    m_toggleButton = new QPushButton(QStringLiteral("Actividad ▸"), this);
    m_toggleButton->setProperty("variant", QStringLiteral("ghost"));
    m_clearButton = new QPushButton(QStringLiteral("Limpiar"), this);
    m_clearButton->setProperty("variant", QStringLiteral("ghost"));

    bar->addWidget(m_toggleButton);
    bar->addStretch(1);
    bar->addWidget(m_clearButton);
    root->addLayout(bar);

    m_content = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(4);

    m_list = new QListWidget(m_content);
    m_list->setAlternatingRowColors(true);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    contentLayout->addWidget(m_list);

    m_content->setVisible(false);
    root->addWidget(m_content);

    connect(m_toggleButton, &QPushButton::clicked, this, [this]() {
        m_expanded = !m_expanded;
        m_content->setVisible(m_expanded);
        m_toggleButton->setText(m_expanded ? QStringLiteral("Actividad ▾") : QStringLiteral("Actividad ▸"));
    });

    connect(m_clearButton, &QPushButton::clicked, this, &ActivityDrawerWidget::clearRequested);
}

void ActivityDrawerWidget::setItems(const QVector<ActivityItem> &items)
{
    m_list->clear();

    for (const ActivityItem &entry : items) {
        const QString line = QStringLiteral("[%1] %2 %3")
                                 .arg(entry.time.trimmed().isEmpty() ? QStringLiteral("--:--:--") : entry.time)
                                 .arg(entry.kind.trimmed().isEmpty() ? QStringLiteral("INFO") : entry.kind)
                                 .arg(entry.message);

        auto *item = new QListWidgetItem(line, m_list);

        const QString kind = entry.kind.toUpper();
        if (kind == QStringLiteral("ERR") || kind == QStringLiteral("ERROR")) {
            item->setForeground(QColor(QStringLiteral("#E07C7C")));
        } else if (kind == QStringLiteral("SYNC") || kind == QStringLiteral("NEW") || kind == QStringLiteral("UPD")) {
            item->setForeground(QColor(QStringLiteral("#8EA2FF")));
        } else if (kind == QStringLiteral("FILE") || kind == QStringLiteral("SAVE")) {
            item->setForeground(QColor(QStringLiteral("#8FD19E")));
        } else if (kind == QStringLiteral("EXPORT") || kind == QStringLiteral("GDOC") || kind == QStringLiteral("GSHT")
                   || kind == QStringLiteral("GSLD") || kind == QStringLiteral("GDRW")) {
            item->setForeground(QColor(QStringLiteral("#86C8E8")));
        } else if (kind == QStringLiteral("LINK") || kind == QStringLiteral("YT") || kind == QStringLiteral("FORM")) {
            item->setForeground(QColor(QStringLiteral("#C7A6FF")));
        }
    }
}
