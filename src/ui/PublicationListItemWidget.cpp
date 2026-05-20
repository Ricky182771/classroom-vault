#include "PublicationListItemWidget.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

static QString badgeStyleForType(const QString &type)
{
    QString bg;
    if (type == QLatin1String("driveFile")) {
        bg = QStringLiteral("#2d5a8e");
    } else if (type == QLatin1String("youtubeVideo")) {
        bg = QStringLiteral("#7a2020");
    } else if (type == QLatin1String("form")) {
        bg = QStringLiteral("#5a3a7a");
    } else {
        bg = QStringLiteral("#1e6644");
    }
    return QStringLiteral(
        "padding:2px 6px;border-radius:4px;background:%1;"
        "color:#D0D3DB;font-size:10px;font-weight:600;").arg(bg);
}

static QString labelForType(const QString &type)
{
    if (type == QLatin1String("driveFile"))    return QStringLiteral("Drive");
    if (type == QLatin1String("youtubeVideo")) return QStringLiteral("YouTube");
    if (type == QLatin1String("form"))         return QStringLiteral("Form");
    return QStringLiteral("Enlace");
}

PublicationListItemWidget::PublicationListItemWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("AssignmentItem"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(8);

    // Header row: kind badge + title
    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    m_kindLabel = new QLabel(QStringLiteral("Aviso"), this);
    m_kindLabel->setStyleSheet(
        QStringLiteral("padding:3px 7px;border-radius:6px;background:#4a4a6a;"
                       "color:#ccc;font-size:11px;font-weight:600;"));
    m_kindLabel->setFixedHeight(22);
    topRow->addWidget(m_kindLabel);

    m_titleLabel = new QLabel(QStringLiteral("Publicacion"), this);
    m_titleLabel->setStyleSheet(
        QStringLiteral("font-weight:700;font-size:14px;background:transparent;border:none;"));
    topRow->addWidget(m_titleLabel, 1);

    root->addLayout(topRow);

    // Full publication text — white, word-wrapped
    m_previewLabel = new QLabel(this);
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setStyleSheet(QStringLiteral("color:#F1F1F1;font-size:13px;"));
    root->addWidget(m_previewLabel);

    // Meta line (date · attachments count)
    m_metaLabel = new QLabel(this);
    m_metaLabel->setProperty("muted", true);
    root->addWidget(m_metaLabel);

    // Action buttons
    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(6);

    m_openFolderButton = new QPushButton(QStringLiteral("Abrir carpeta"), this);
    m_openClassroomButton = new QPushButton(QStringLiteral("Classroom"), this);

    buttons->addWidget(m_openFolderButton);
    buttons->addWidget(m_openClassroomButton);
    buttons->addStretch(1);

    root->addLayout(buttons);

    // Attachments section (hidden when empty)
    m_attachmentsContainer = new QWidget(this);
    m_attachmentsLayout = new QVBoxLayout(m_attachmentsContainer);
    m_attachmentsLayout->setContentsMargins(0, 4, 0, 0);
    m_attachmentsLayout->setSpacing(4);
    m_attachmentsContainer->setVisible(false);
    root->addWidget(m_attachmentsContainer);

    connect(m_openFolderButton, &QPushButton::clicked, this, [this]() {
        emit openFolderRequested(m_data.courseId, m_data.publicationId);
    });
    connect(m_openClassroomButton, &QPushButton::clicked, this, [this]() {
        emit openClassroomRequested(m_data.courseId, m_data.publicationId);
    });
}

void PublicationListItemWidget::setData(const PublicationListItemData &data)
{
    m_data = data;

    m_kindLabel->setText(data.kindLabel.trimmed().isEmpty()
                             ? QStringLiteral("Aviso") : data.kindLabel.trimmed());
    m_titleLabel->setText(data.title.trimmed().isEmpty()
                              ? QStringLiteral("Sin titulo") : data.title.trimmed());

    const QString preview = data.textPreview.trimmed();
    m_previewLabel->setText(preview.isEmpty() ? QStringLiteral("Sin contenido.") : preview);
    m_previewLabel->setVisible(!preview.isEmpty());

    QString meta = data.createdAtText.trimmed();
    if (!data.materials.isEmpty()) {
        meta += QStringLiteral("  ·  Adjuntos: %1").arg(data.materials.size());
    }
    m_metaLabel->setText(meta);

    m_openFolderButton->setEnabled(!data.folderPath.trimmed().isEmpty());
    m_openClassroomButton->setEnabled(!data.classroomUrl.trimmed().isEmpty());

    // Rebuild attachment rows
    while (QLayoutItem *child = m_attachmentsLayout->takeAt(0)) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    if (!data.materials.isEmpty()) {
        auto *sep = new QFrame(m_attachmentsContainer);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet(QStringLiteral("color:#3A3D46;margin:2px 0;"));
        m_attachmentsLayout->addWidget(sep);

        for (const AssignmentMaterial &mat : data.materials) {
            auto *row = new QWidget(m_attachmentsContainer);
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 2, 0, 2);
            rowLayout->setSpacing(8);

            auto *badge = new QLabel(labelForType(mat.type), row);
            badge->setStyleSheet(badgeStyleForType(mat.type));
            badge->setFixedHeight(18);
            rowLayout->addWidget(badge);

            const QString matTitle = mat.title.trimmed().isEmpty()
                ? QStringLiteral("Sin titulo") : mat.title.trimmed();
            auto *titleLabel = new QLabel(matTitle, row);
            titleLabel->setStyleSheet(QStringLiteral("font-size:12px;color:#D0D3DB;"));
            titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            rowLayout->addWidget(titleLabel, 1);

            const QString url = mat.alternateLink.isEmpty() ? mat.url : mat.alternateLink;
            if (!url.isEmpty()) {
                const QString btnLabel = (mat.type == QLatin1String("driveFile"))
                    ? QStringLiteral("Descargar")
                    : QStringLiteral("Abrir");
                auto *downloadBtn = new QPushButton(btnLabel, row);
                downloadBtn->setProperty("variant", QStringLiteral("ghost"));
                downloadBtn->setFixedHeight(26);
                connect(downloadBtn, &QPushButton::clicked, this,
                        [this, url, matTitle]() {
                            emit downloadAttachmentRequested(
                                m_data.courseId, m_data.publicationId, url, matTitle);
                        });
                rowLayout->addWidget(downloadBtn);
            }

            m_attachmentsLayout->addWidget(row);
        }
    }

    m_attachmentsContainer->setVisible(!data.materials.isEmpty());
}

PublicationListItemData PublicationListItemWidget::data() const
{
    return m_data;
}
