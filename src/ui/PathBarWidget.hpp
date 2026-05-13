#pragma once

#include <QFrame>

class QLabel;
class QPushButton;

class PathBarWidget : public QFrame {
    Q_OBJECT

public:
    explicit PathBarWidget(QWidget *parent = nullptr);

    void setBasePath(const QString &basePath);
    void setLastSyncText(const QString &text);

signals:
    void changeBasePathRequested();
    void openBasePathRequested();
    void syncAllRequested();
    void downloadAttachmentsRequested();

private:
    QLabel *m_pathValue = nullptr;
    QLabel *m_lastSyncValue = nullptr;
    QPushButton *m_changePathButton = nullptr;
    QPushButton *m_openBasePathButton = nullptr;
    QPushButton *m_syncAllButton = nullptr;
    QPushButton *m_downloadAttachmentsButton = nullptr;
};
