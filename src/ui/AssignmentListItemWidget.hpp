#pragma once

#include "UiModels.hpp"

#include <QFrame>

class QLabel;
class QPushButton;

class AssignmentListItemWidget : public QFrame {
    Q_OBJECT

public:
    explicit AssignmentListItemWidget(QWidget *parent = nullptr);

    void setData(const AssignmentListItemData &data);
    AssignmentListItemData data() const;

signals:
    void selected(const QString &courseId, const QString &assignmentId);
    void openFolderRequested(const QString &courseId, const QString &assignmentId);
    void openClassroomRequested(const QString &courseId, const QString &assignmentId);

private:
    AssignmentListItemData m_data;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_dueLabel = nullptr;
    QLabel *m_stateLabel = nullptr;
    QLabel *m_visualStatusBadge = nullptr;
    QLabel *m_metaLabel = nullptr;
    QLabel *m_attachLabel = nullptr;
    QLabel *m_syncLabel = nullptr;
    QLabel *m_descriptionLabel = nullptr;

    QPushButton *m_openButton = nullptr;
    QPushButton *m_openFolderButton = nullptr;
    QPushButton *m_openClassroomButton = nullptr;
};
