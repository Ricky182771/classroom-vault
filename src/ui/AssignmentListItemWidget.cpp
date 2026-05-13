#include "AssignmentListItemWidget.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

AssignmentListItemWidget::AssignmentListItemWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("AssignmentItem"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(8);

    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    auto *icon = new QLabel(QStringLiteral("[T]"), this);
    icon->setFixedWidth(22);
    topRow->addWidget(icon);

    auto *titleCol = new QVBoxLayout();
    titleCol->setSpacing(3);

    m_titleLabel = new QLabel(QStringLiteral("Tarea"), this);
    m_titleLabel->setStyleSheet(QStringLiteral("font-weight:700;font-size:14px;background:transparent;border:none;"));
    titleCol->addWidget(m_titleLabel);

    m_descriptionLabel = new QLabel(QStringLiteral("Sin descripcion"), this);
    m_descriptionLabel->setProperty("subtle", true);
    m_descriptionLabel->setWordWrap(true);
    titleCol->addWidget(m_descriptionLabel);

    topRow->addLayout(titleCol, 1);

    m_dueLabel = new QLabel(QStringLiteral("Sin fecha"), this);
    m_dueLabel->setProperty("muted", true);
    topRow->addWidget(m_dueLabel);

    root->addLayout(topRow);

    auto *metaRow = new QHBoxLayout();
    metaRow->setSpacing(10);

    m_stateLabel = new QLabel(QStringLiteral("Estado: —"), this);
    m_metaLabel = new QLabel(QStringLiteral("Metadata: —"), this);
    m_attachLabel = new QLabel(QStringLiteral("Adjuntos: 0"), this);
    m_syncLabel = new QLabel(QStringLiteral("Ultima sync: —"), this);

    m_stateLabel->setProperty("muted", true);
    m_metaLabel->setProperty("muted", true);
    m_attachLabel->setProperty("muted", true);
    m_syncLabel->setProperty("subtle", true);

    metaRow->addWidget(m_stateLabel);
    metaRow->addWidget(m_metaLabel);
    metaRow->addWidget(m_attachLabel);
    metaRow->addWidget(m_syncLabel, 1);

    root->addLayout(metaRow);

    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(6);

    m_openButton = new QPushButton(QStringLiteral("Abrir detalle"), this);
    m_openButton->setProperty("variant", QStringLiteral("primary"));
    m_openFolderButton = new QPushButton(QStringLiteral("Abrir carpeta"), this);
    m_openClassroomButton = new QPushButton(QStringLiteral("Classroom"), this);
    m_downloadButton = new QPushButton(QStringLiteral("Adjuntos"), this);

    buttons->addWidget(m_openButton);
    buttons->addWidget(m_openFolderButton);
    buttons->addWidget(m_openClassroomButton);
    buttons->addWidget(m_downloadButton);
    buttons->addStretch(1);

    root->addLayout(buttons);

    connect(m_openButton, &QPushButton::clicked, this, [this]() {
        emit selected(m_data.courseId, m_data.assignmentId);
    });
    connect(m_openFolderButton, &QPushButton::clicked, this, [this]() {
        emit openFolderRequested(m_data.courseId, m_data.assignmentId);
    });
    connect(m_openClassroomButton, &QPushButton::clicked, this, [this]() {
        emit openClassroomRequested(m_data.courseId, m_data.assignmentId);
    });
    connect(m_downloadButton, &QPushButton::clicked, this, [this]() {
        emit downloadAttachmentsRequested(m_data.courseId, m_data.assignmentId);
    });
}

void AssignmentListItemWidget::setData(const AssignmentListItemData &data)
{
    m_data = data;

    m_titleLabel->setText(data.title.trimmed().isEmpty() ? QStringLiteral("Tarea sin titulo") : data.title.trimmed());
    m_dueLabel->setText(data.dueDateText.trimmed().isEmpty() ? QStringLiteral("Sin fecha") : data.dueDateText.trimmed());
    m_stateLabel->setText(QStringLiteral("Estado: %1").arg(data.stateText.trimmed().isEmpty() ? QStringLiteral("—") : data.stateText.trimmed()));
    m_metaLabel->setText(QStringLiteral("Metadata: %1").arg(data.metadataStatus.trimmed().isEmpty() ? QStringLiteral("—") : data.metadataStatus.trimmed()));
    m_attachLabel->setText(QStringLiteral("Adjuntos: %1 (%2)").arg(data.attachmentsCount).arg(data.attachmentsStatus.trimmed().isEmpty() ? QStringLiteral("—") : data.attachmentsStatus.trimmed()));
    m_syncLabel->setText(QStringLiteral("Ultima sync: %1").arg(data.lastSyncText.trimmed().isEmpty() ? QStringLiteral("—") : data.lastSyncText.trimmed()));

    QString preview = data.descriptionPreview.trimmed();
    if (preview.isEmpty()) {
        preview = QStringLiteral("Esta tarea no tiene descripcion guardada.");
    }
    m_descriptionLabel->setText(preview);

    m_openFolderButton->setEnabled(!data.folderPath.trimmed().isEmpty());
    m_openClassroomButton->setEnabled(!data.classroomUrl.trimmed().isEmpty());
}

AssignmentListItemData AssignmentListItemWidget::data() const
{
    return m_data;
}
