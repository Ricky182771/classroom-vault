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

    void setPageTitle(const QString &title);
    void setConnectionState(const QString &state);
    void setConnectedEmail(const QString &email);
    void setSearchPlaceholder(const QString &placeholder);

signals:
    void syncRequested();
    void accountRequested();
    void searchRequested(const QString &text);
    void searchTextChanged(const QString &text);

private:
    QLabel *m_titleLabel = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_connectionLabel = nullptr;
    QLabel *m_emailLabel = nullptr;
    QPushButton *m_syncButton = nullptr;
    QPushButton *m_accountButton = nullptr;
};
