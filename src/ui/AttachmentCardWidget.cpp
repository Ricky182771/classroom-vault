#include "AttachmentCardWidget.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

AttachmentCardWidget::AttachmentCardWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("AttachmentCard"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto *top = new QHBoxLayout();
    top->setSpacing(8);

    m_iconLabel = new QLabel(QStringLiteral("[F]"), this);
    m_iconLabel->setFixedWidth(30);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setObjectName(QStringLiteral("Section"));
    m_iconLabel->setStyleSheet(QStringLiteral("border-radius:6px;padding:4px;"));
    top->addWidget(m_iconLabel);

    auto *labelsCol = new QVBoxLayout();
    labelsCol->setSpacing(3);

    m_titleLabel = new QLabel(QStringLiteral("Adjunto"), this);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setStyleSheet(QStringLiteral("font-weight:600;"));
    labelsCol->addWidget(m_titleLabel);

    m_infoLabel = new QLabel(QStringLiteral("Tipo"), this);
    m_infoLabel->setProperty("muted", true);
    labelsCol->addWidget(m_infoLabel);

    top->addLayout(labelsCol, 1);

    layout->addLayout(top);

    m_statusLabel = new QLabel(QStringLiteral("Estado"), this);
    m_statusLabel->setProperty("subtle", true);
    layout->addWidget(m_statusLabel);

    auto *actions = new QHBoxLayout();
    actions->setSpacing(6);

    m_openButton = new QPushButton(QStringLiteral("Abrir"), this);
    m_openFolderButton = new QPushButton(QStringLiteral("Carpeta"), this);
    m_openUrlButton = new QPushButton(QStringLiteral("Enlace"), this);

    actions->addWidget(m_openButton);
    actions->addWidget(m_openFolderButton);
    actions->addWidget(m_openUrlButton);
    actions->addStretch(1);

    layout->addLayout(actions);

    connect(m_openButton, &QPushButton::clicked, this, [this]() {
        emit openRequested(m_attachment);
    });
    connect(m_openFolderButton, &QPushButton::clicked, this, [this]() {
        emit openFolderRequested(m_attachment);
    });
    connect(m_openUrlButton, &QPushButton::clicked, this, [this]() {
        emit openUrlRequested(m_attachment);
    });
}

void AttachmentCardWidget::setAttachment(const AttachmentUiState &attachment)
{
    m_attachment = attachment;

    const QString displayTitle = attachment.title.trimmed().isEmpty()
        ? (attachment.fileName.trimmed().isEmpty() ? QStringLiteral("Adjunto") : attachment.fileName.trimmed())
        : attachment.title.trimmed();

    m_iconLabel->setText(iconForAttachment(attachment));
    m_titleLabel->setText(displayTitle);

    QString typeText = attachment.type.trimmed();
    if (typeText.isEmpty()) {
        typeText = attachment.sourceMimeType.trimmed();
    }
    if (typeText.isEmpty()) {
        typeText = QStringLiteral("Desconocido");
    }

    m_infoLabel->setText(QStringLiteral("%1 · %2").arg(typeText, attachment.sizeText.trimmed().isEmpty() ? QStringLiteral("Sin tamano") : attachment.sizeText.trimmed()));
    m_statusLabel->setText(QStringLiteral("Estado: %1").arg(attachment.status.trimmed().isEmpty() ? QStringLiteral("Pendiente") : attachment.status.trimmed()));

    const bool hasLocal = !attachment.localPath.trimmed().isEmpty();
    const bool hasUrl = !attachment.url.trimmed().isEmpty();

    m_openButton->setEnabled((hasLocal && attachment.existsLocally) || hasUrl);
    m_openFolderButton->setEnabled(hasLocal);
    m_openUrlButton->setEnabled(hasUrl);
}

AttachmentUiState AttachmentCardWidget::attachment() const
{
    return m_attachment;
}

QString AttachmentCardWidget::iconForAttachment(const AttachmentUiState &attachment) const
{
    const QString lowerName = attachment.fileName.toLower();
    const QString lowerMime = attachment.sourceMimeType.toLower();
    const QString lowerType = attachment.type.toLower();

    if (lowerType == QStringLiteral("link")) {
        return QStringLiteral("[L]");
    }
    if (lowerType == QStringLiteral("youtubevideo") || lowerName.contains(QStringLiteral("youtube"))) {
        return QStringLiteral("[Y]");
    }
    if (lowerType == QStringLiteral("form") || lowerMime.contains(QStringLiteral("google-apps.form"))) {
        return QStringLiteral("[F]");
    }
    if (lowerName.endsWith(QStringLiteral(".pdf"))) {
        return QStringLiteral("[PDF]");
    }
    if (lowerName.endsWith(QStringLiteral(".docx"))) {
        return QStringLiteral("[DOC]");
    }
    if (lowerName.endsWith(QStringLiteral(".xlsx"))) {
        return QStringLiteral("[XLS]");
    }
    if (lowerName.endsWith(QStringLiteral(".pptx"))) {
        return QStringLiteral("[PPT]");
    }
    if (lowerName.endsWith(QStringLiteral(".png"))
        || lowerName.endsWith(QStringLiteral(".jpg"))
        || lowerName.endsWith(QStringLiteral(".jpeg"))) {
        return QStringLiteral("[IMG]");
    }

    return QStringLiteral("[A]");
}
