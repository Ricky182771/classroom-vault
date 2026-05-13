#pragma once

#include <QFrame>

class QLabel;
class QProgressBar;

class StatusBarWidget : public QFrame {
    Q_OBJECT

public:
    explicit StatusBarWidget(QWidget *parent = nullptr);

    void setStatusText(const QString &text);
    void setLastActionText(const QString &text);
    void setErrorCount(int errors);
    void setProgress(int current, int total);

private:
    QLabel *m_statusLabel = nullptr;
    QLabel *m_lastActionLabel = nullptr;
    QLabel *m_errorLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
};
