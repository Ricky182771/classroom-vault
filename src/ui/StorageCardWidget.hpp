#pragma once

#include <QFrame>

class QLabel;
class QPushButton;

class StorageCardWidget : public QFrame {
    Q_OBJECT

public:
    explicit StorageCardWidget(QWidget *parent = nullptr);

    void setBasePath(const QString &path);
    void setUsedText(const QString &usedText);
    void setAttachmentsCount(int attachments);

signals:
    void openBasePathRequested();

private:
    QLabel *m_pathLabel = nullptr;
    QLabel *m_usedLabel = nullptr;
    QLabel *m_attachmentsLabel = nullptr;
    QPushButton *m_openButton = nullptr;
};
