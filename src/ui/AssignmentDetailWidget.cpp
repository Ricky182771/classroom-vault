#include "AssignmentDetailWidget.hpp"

#include "AttachmentCardWidget.hpp"

#include <QDesktopServices>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>
#include <utility>

AssignmentDetailWidget::AssignmentDetailWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    auto *headerCard = new QFrame(this);
    headerCard->setObjectName(QStringLiteral("Section"));

    auto *headerLayout = new QVBoxLayout(headerCard);
    headerLayout->setContentsMargins(12, 12, 12, 12);
    headerLayout->setSpacing(8);

    auto *topRow = new QHBoxLayout();
    m_backButton = new QPushButton(QStringLiteral("<- Volver a tareas"), headerCard);
    m_backButton->setProperty("variant", QStringLiteral("ghost"));
    topRow->addWidget(m_backButton);
    topRow->addStretch(1);
    headerLayout->addLayout(topRow);

    m_titleLabel = new QLabel(QStringLiteral("Tarea"), headerCard);
    m_titleLabel->setStyleSheet(QStringLiteral("font-size:20px;font-weight:700;background:transparent;border:none;"));
    m_titleLabel->setWordWrap(true);
    headerLayout->addWidget(m_titleLabel);

    m_courseLabel = new QLabel(QStringLiteral("Materia: —"), headerCard);
    m_courseLabel->setProperty("muted", true);
    headerLayout->addWidget(m_courseLabel);

    m_dueLabel = new QLabel(QStringLiteral("Entrega: Sin fecha"), headerCard);
    m_dueLabel->setProperty("muted", true);
    headerLayout->addWidget(m_dueLabel);

    m_stateLabel = new QLabel(QStringLiteral("Estado: —"), headerCard);
    m_stateLabel->setProperty("muted", true);
    headerLayout->addWidget(m_stateLabel);

    m_typeLabel = new QLabel(QStringLiteral("Tipo: —"), headerCard);
    m_typeLabel->setProperty("muted", true);
    headerLayout->addWidget(m_typeLabel);

    auto *actions = new QHBoxLayout();
    actions->setSpacing(6);

    m_openClassroomButton = new QPushButton(QStringLiteral("Abrir en Classroom"), headerCard);
    m_openFolderButton = new QPushButton(QStringLiteral("Abrir carpeta local"), headerCard);
    m_resyncButton = new QPushButton(QStringLiteral("Re-sincronizar metadata"), headerCard);
    m_downloadButton = new QPushButton(QStringLiteral("Descargar adjuntos"), headerCard);

    actions->addWidget(m_openClassroomButton);
    actions->addWidget(m_openFolderButton);
    actions->addWidget(m_resyncButton);
    actions->addWidget(m_downloadButton);
    actions->addStretch(1);

    headerLayout->addLayout(actions);

    root->addWidget(headerCard);

    auto *descriptionCard = new QFrame(this);
    descriptionCard->setObjectName(QStringLiteral("Section"));
    auto *descriptionLayout = new QVBoxLayout(descriptionCard);
    descriptionLayout->setContentsMargins(12, 12, 12, 12);
    descriptionLayout->setSpacing(8);

    auto *descriptionTitle = new QLabel(QStringLiteral("Descripcion"), descriptionCard);
    descriptionTitle->setStyleSheet(QStringLiteral("font-weight:700;background:transparent;border:none;"));
    descriptionLayout->addWidget(descriptionTitle);

    m_descriptionLabel = new QLabel(QStringLiteral("Esta tarea no tiene descripcion guardada."), descriptionCard);
    m_descriptionLabel->setWordWrap(true);
    descriptionLayout->addWidget(m_descriptionLabel);

    auto *evidenceTitle = new QLabel(QStringLiteral("Evidencia local"), descriptionCard);
    evidenceTitle->setStyleSheet(QStringLiteral("font-weight:700;background:transparent;border:none;"));
    descriptionLayout->addWidget(evidenceTitle);

    m_evidenceLabel = new QLabel(QStringLiteral("Sin evidencia"), descriptionCard);
    m_evidenceLabel->setProperty("subtle", true);
    m_evidenceLabel->setWordWrap(true);
    descriptionLayout->addWidget(m_evidenceLabel);

    root->addWidget(descriptionCard);

    auto *attachmentsTitle = new QLabel(QStringLiteral("Adjuntos"), this);
    attachmentsTitle->setStyleSheet(QStringLiteral("font-size:16px;font-weight:700;background:transparent;border:none;"));
    root->addWidget(attachmentsTitle);

    m_attachmentsScroll = new QScrollArea(this);
    m_attachmentsScroll->setWidgetResizable(true);
    m_attachmentsScroll->setFrameShape(QFrame::NoFrame);

    m_attachmentsContainer = new QWidget(m_attachmentsScroll);
    m_attachmentsLayout = new QVBoxLayout(m_attachmentsContainer);
    m_attachmentsLayout->setContentsMargins(0, 0, 0, 0);
    m_attachmentsLayout->setSpacing(8);
    m_attachmentsLayout->addStretch(1);

    m_attachmentsScroll->setWidget(m_attachmentsContainer);
    root->addWidget(m_attachmentsScroll, 1);

    connect(m_backButton, &QPushButton::clicked, this, &AssignmentDetailWidget::backRequested);
    connect(m_openClassroomButton, &QPushButton::clicked, this, [this]() {
        emit openClassroomRequested(m_preview.courseId, m_preview.assignmentId);
    });
    connect(m_openFolderButton, &QPushButton::clicked, this, [this]() {
        emit openFolderRequested(m_preview.courseId, m_preview.assignmentId);
    });
    connect(m_resyncButton, &QPushButton::clicked, this, [this]() {
        emit resyncMetadataRequested(m_preview.courseId, m_preview.assignmentId);
    });
    connect(m_downloadButton, &QPushButton::clicked, this, [this]() {
        emit downloadAttachmentsRequested(m_preview.courseId, m_preview.assignmentId);
    });
}

void AssignmentDetailWidget::setPreviewData(const AssignmentPreviewData &preview)
{
    m_preview = preview;

    m_titleLabel->setText(preview.title.trimmed().isEmpty() ? QStringLiteral("Tarea sin titulo") : preview.title.trimmed());
    m_courseLabel->setText(QStringLiteral("Materia: %1").arg(preview.courseName.trimmed().isEmpty() ? QStringLiteral("—") : preview.courseName.trimmed()));
    m_dueLabel->setText(QStringLiteral("Entrega: %1 %2").arg(preview.dueDateText, preview.dueTimeText));
    m_stateLabel->setText(QStringLiteral("Estado: %1").arg(preview.state.trimmed().isEmpty() ? QStringLiteral("—") : preview.state.trimmed()));
    m_typeLabel->setText(QStringLiteral("Tipo: %1").arg(preview.workType.trimmed().isEmpty() ? QStringLiteral("—") : preview.workType.trimmed()));

    QString description = preview.description.trimmed();
    if (description.isEmpty()) {
        description = QStringLiteral("Esta tarea no tiene descripcion guardada.");
    }
    m_descriptionLabel->setText(description);

    m_evidenceLabel->setText(
        QStringLiteral("metadata.json: %1\nRuta local: %2\nFecha de respaldo: %3\nAdjuntos detectados: %4")
            .arg(preview.metadataPath.trimmed().isEmpty() ? QStringLiteral("No encontrado") : preview.metadataPath.trimmed())
            .arg(preview.localFolderPath.trimmed().isEmpty() ? QStringLiteral("—") : preview.localFolderPath.trimmed())
            .arg(preview.syncedAt.trimmed().isEmpty() ? QStringLiteral("—") : preview.syncedAt.trimmed())
            .arg(preview.attachments.size()));

    m_openClassroomButton->setEnabled(!preview.alternateLink.trimmed().isEmpty());
    m_openFolderButton->setEnabled(!preview.localFolderPath.trimmed().isEmpty());

    rebuildAttachments();
}

AssignmentPreviewData AssignmentDetailWidget::previewData() const
{
    return m_preview;
}

void AssignmentDetailWidget::rebuildAttachments()
{
    QLayoutItem *item = nullptr;
    while ((item = m_attachmentsLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (m_preview.attachments.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("Esta tarea no tiene adjuntos registrados."), m_attachmentsContainer);
        empty->setProperty("subtle", true);
        m_attachmentsLayout->addWidget(empty);
        m_attachmentsLayout->addStretch(1);
        return;
    }

    for (const AttachmentUiState &attachment : std::as_const(m_preview.attachments)) {
        auto *card = new AttachmentCardWidget(m_attachmentsContainer);
        card->setAttachment(attachment);

        connect(card, &AttachmentCardWidget::openRequested, this, [this](const AttachmentUiState &entry) {
            if (entry.existsLocally && !entry.localPath.trimmed().isEmpty()) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(entry.localPath));
                return;
            }
            if (!entry.url.trimmed().isEmpty()) {
                QDesktopServices::openUrl(QUrl(entry.url));
                return;
            }
            QMessageBox::information(this, QStringLiteral("Adjunto"), QStringLiteral("No hay archivo local ni enlace disponible."));
        });

        connect(card, &AttachmentCardWidget::openFolderRequested, this, [this](const AttachmentUiState &entry) {
            if (entry.localPath.trimmed().isEmpty()) {
                QMessageBox::information(this, QStringLiteral("Adjunto"), QStringLiteral("No hay ruta local para este adjunto."));
                return;
            }
            const QString folder = QFileInfo(entry.localPath).absolutePath();
            if (folder.trimmed().isEmpty()) {
                QMessageBox::information(this, QStringLiteral("Adjunto"), QStringLiteral("No se pudo determinar la carpeta local."));
                return;
            }
            QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
        });

        connect(card, &AttachmentCardWidget::openUrlRequested, this, [this](const AttachmentUiState &entry) {
            if (entry.url.trimmed().isEmpty()) {
                QMessageBox::information(this, QStringLiteral("Adjunto"), QStringLiteral("Este adjunto no tiene enlace original."));
                return;
            }
            QDesktopServices::openUrl(QUrl(entry.url));
        });

        m_attachmentsLayout->addWidget(card);
    }

    m_attachmentsLayout->addStretch(1);
}
