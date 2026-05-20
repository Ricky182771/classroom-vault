#pragma once

#include "UiModels.hpp"

#include <QFrame>
#include <QVBoxLayout>

class QLabel;
class QPushButton;

class PublicationListItemWidget : public QFrame {
    Q_OBJECT

public:
    explicit PublicationListItemWidget(QWidget *parent = nullptr);

    void setData(const PublicationListItemData &data);
    PublicationListItemData data() const;

signals:
    void openFolderRequested(const QString &courseId, const QString &publicationId);
    void openClassroomRequested(const QString &courseId, const QString &publicationId);
    void downloadAttachmentRequested(const QString &courseId, const QString &publicationId,
                                     const QString &url, const QString &title);

private:
    PublicationListItemData m_data;

    QLabel *m_kindLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_previewLabel = nullptr;
    QLabel *m_metaLabel = nullptr;

    QPushButton *m_openFolderButton = nullptr;
    QPushButton *m_openClassroomButton = nullptr;

    QWidget *m_attachmentsContainer = nullptr;
    QVBoxLayout *m_attachmentsLayout = nullptr;
};
