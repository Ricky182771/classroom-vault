#pragma once

#include <QFrame>
#include <QString>

class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;

class TopBarWidget : public QFrame {
    Q_OBJECT

public:
    explicit TopBarWidget(QWidget *parent = nullptr);

    void setPageTitle(const QString &title);
    void setConnectionState(const QString &state);
    void setConnectedEmail(const QString &email);
    void setSearchPlaceholder(const QString &placeholder);
    void setGlobalSemesterFilter(const QString &semester);
    QString globalSemesterFilter() const;

signals:
    void syncRequested();
    void accountRequested();
    void searchRequested(const QString &text);
    void searchTextChanged(const QString &text);
    void globalSemesterFilterChanged(const QString &semester);

private:
    QLabel *m_titleLabel = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_connectionLabel = nullptr;
    QLabel *m_emailLabel = nullptr;
    QComboBox *m_semesterCombo = nullptr;
    QPushButton *m_syncButton = nullptr;
    QPushButton *m_accountButton = nullptr;
};
