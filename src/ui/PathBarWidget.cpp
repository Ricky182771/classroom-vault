#include "PathBarWidget.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

PathBarWidget::PathBarWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("PathBar"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    auto *label = new QLabel(QStringLiteral("Ruta base:"), this);
    label->setProperty("subtle", true);
    layout->addWidget(label);

    m_pathValue = new QLabel(QStringLiteral("(sin configurar)"), this);
    m_pathValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_pathValue, 1);

    m_changePathButton = new QPushButton(QStringLiteral("Cambiar ruta"), this);
    layout->addWidget(m_changePathButton);

    m_openBasePathButton = new QPushButton(QStringLiteral("Abrir respaldo"), this);
    layout->addWidget(m_openBasePathButton);

    m_syncAllButton = new QPushButton(QStringLiteral("Sincronizar todo"), this);
    m_syncAllButton->setProperty("variant", QStringLiteral("primary"));
    layout->addWidget(m_syncAllButton);

    auto *lastSyncLabel = new QLabel(QStringLiteral("Ultima sync:"), this);
    lastSyncLabel->setProperty("subtle", true);
    layout->addWidget(lastSyncLabel);

    m_lastSyncValue = new QLabel(QStringLiteral("—"), this);
    layout->addWidget(m_lastSyncValue);

    connect(m_changePathButton, &QPushButton::clicked, this, &PathBarWidget::changeBasePathRequested);
    connect(m_openBasePathButton, &QPushButton::clicked, this, &PathBarWidget::openBasePathRequested);
    connect(m_syncAllButton, &QPushButton::clicked, this, &PathBarWidget::syncAllRequested);
}

void PathBarWidget::setBasePath(const QString &basePath)
{
    const QString clean = basePath.trimmed();
    m_pathValue->setText(clean.isEmpty() ? QStringLiteral("(sin configurar)") : clean);
}

void PathBarWidget::setLastSyncText(const QString &text)
{
    const QString clean = text.trimmed();
    m_lastSyncValue->setText(clean.isEmpty() ? QStringLiteral("—") : clean);
}
