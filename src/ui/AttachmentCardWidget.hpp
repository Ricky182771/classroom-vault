#pragma once

#include "UiModels.hpp"

#include <QFrame>

class QLabel;
class QPushButton;

class AttachmentCardWidget : public QFrame {
    Q_OBJECT

public:
    explicit AttachmentCardWidget(QWidget *parent = nullptr);

    void setAttachment(const AttachmentUiState &attachment);
    AttachmentUiState attachment() const;

signals:
    void openRequested(const AttachmentUiState &attachment);
    void openFolderRequested(const AttachmentUiState &attachment);
    void openUrlRequested(const AttachmentUiState &attachment);

private:
    QString iconForAttachment(const AttachmentUiState &attachment) const;

    AttachmentUiState m_attachment;

    QLabel *m_iconLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_infoLabel = nullptr;
    QLabel *m_statusLabel = nullptr;

    QPushButton *m_openButton = nullptr;
    QPushButton *m_openFolderButton = nullptr;
    QPushButton *m_openUrlButton = nullptr;
};
