#pragma once

#include <QFrame>
#include <QString>

class QLabel;
class QLineEdit;
class QPushButton;

class TopBarWidget : public QFrame {
    Q_OBJECT

public:
    explicit TopBarWidget(QWidget *parent = nullptr);

    void setBreadcrumb(const QString &left, const QString &right);
    void setConnectionState(const QString &state);
    void setConnectedEmail(const QString &email);

signals:
    void syncRequested();
    void settingsRequested();
    void searchRequested(const QString &text);
    void toggleSidebarRequested();

private:
    QLabel *m_breadcrumbLeft = nullptr;
    QLabel *m_breadcrumbRight = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_connectionLabel = nullptr;
    QLabel *m_emailLabel = nullptr;
    QPushButton *m_toggleSidebarButton = nullptr;
    QPushButton *m_syncButton = nullptr;
    QPushButton *m_settingsButton = nullptr;
};
