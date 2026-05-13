#pragma once

#include <QFrame>

class QLabel;
class QLineEdit;
class QPushButton;
class QCheckBox;

class SettingsViewWidget : public QFrame {
    Q_OBJECT

public:
    explicit SettingsViewWidget(QWidget *parent = nullptr);

    void setBasePath(const QString &path);
    void setConnectedEmail(const QString &email);
    void setTokenStatus(const QString &status);
    void setAutoDownloadAttachments(bool enabled);

signals:
    void changeBasePathRequested();
    void loginRequested();
    void logoutRequested();
    void loadClassroomRequested();
    void loadSampleRequested();
    void autoDownloadAttachmentsChanged(bool enabled);

private:
    QLineEdit *m_basePathEdit = nullptr;
    QLabel *m_emailLabel = nullptr;
    QLabel *m_tokenStatusLabel = nullptr;
    QCheckBox *m_autoDownloadCheck = nullptr;
    QPushButton *m_changePathButton = nullptr;
    QPushButton *m_loginButton = nullptr;
    QPushButton *m_logoutButton = nullptr;
    QPushButton *m_loadClassroomButton = nullptr;
    QPushButton *m_loadSampleButton = nullptr;
};
