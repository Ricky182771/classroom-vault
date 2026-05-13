#include "StorageCardWidget.hpp"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

StorageCardWidget::StorageCardWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("StorageCard"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(6);

    auto *title = new QLabel(QStringLiteral("Almacenamiento"), this);
    title->setStyleSheet(QStringLiteral("font-weight: 700;"));
    layout->addWidget(title);

    m_pathLabel = new QLabel(QStringLiteral("Ruta: —"), this);
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setProperty("muted", true);
    layout->addWidget(m_pathLabel);

    m_usedLabel = new QLabel(QStringLiteral("Uso estimado: —"), this);
    m_usedLabel->setProperty("muted", true);
    layout->addWidget(m_usedLabel);

    m_attachmentsLabel = new QLabel(QStringLiteral("Adjuntos: 0"), this);
    m_attachmentsLabel->setProperty("muted", true);
    layout->addWidget(m_attachmentsLabel);

    m_openButton = new QPushButton(QStringLiteral("Abrir respaldo"), this);
    m_openButton->setProperty("variant", QStringLiteral("ghost"));
    layout->addWidget(m_openButton);

    connect(m_openButton, &QPushButton::clicked, this, &StorageCardWidget::openBasePathRequested);
}

void StorageCardWidget::setBasePath(const QString &path)
{
    m_pathLabel->setText(QStringLiteral("Ruta: %1").arg(path.trimmed().isEmpty() ? QStringLiteral("—") : path));
}

void StorageCardWidget::setUsedText(const QString &usedText)
{
    m_usedLabel->setText(QStringLiteral("Uso estimado: %1").arg(usedText.trimmed().isEmpty() ? QStringLiteral("—") : usedText));
}

void StorageCardWidget::setAttachmentsCount(int attachments)
{
    m_attachmentsLabel->setText(QStringLiteral("Adjuntos: %1").arg(attachments < 0 ? 0 : attachments));
}
