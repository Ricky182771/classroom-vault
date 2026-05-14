#pragma once

#include "UiModels.hpp"

#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QScrollArea;
class AttachmentCardWidget;
class UserWorkPanelWidget;

class AssignmentDetailWidget : public QWidget {
    Q_OBJECT

public:
    explicit AssignmentDetailWidget(QWidget *parent = nullptr);

    void setPreviewData(const AssignmentPreviewData &preview);
    AssignmentPreviewData previewData() const;

signals:
    void backRequested();
    void openClassroomRequested(const QString &courseId, const QString &assignmentId);
    void openFolderRequested(const QString &courseId, const QString &assignmentId);
    void resyncMetadataRequested(const QString &courseId, const QString &assignmentId);

private:
    void rebuildAttachments();

    AssignmentPreviewData m_preview;

    QPushButton *m_backButton = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_visualStatusBadge = nullptr;
    QLabel *m_courseLabel = nullptr;
    QLabel *m_dueLabel = nullptr;
    QLabel *m_stateLabel = nullptr;
    QLabel *m_typeLabel = nullptr;

    QPushButton *m_openClassroomButton = nullptr;
    QPushButton *m_openFolderButton = nullptr;
    QPushButton *m_resyncButton = nullptr;

    QLabel *m_descriptionLabel = nullptr;
    QLabel *m_evidenceLabel = nullptr;

    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_scrollContent = nullptr;
    UserWorkPanelWidget *m_userWorkPanel = nullptr;
    QWidget *m_attachmentsContainer = nullptr;
    QVBoxLayout *m_attachmentsLayout = nullptr;
};
