#pragma once

#include <QFrame>

class QLabel;
class QPushButton;

class HeroStripWidget : public QFrame {
    Q_OBJECT

public:
    explicit HeroStripWidget(QWidget *parent = nullptr);

    void setApiStatusText(const QString &text);

signals:
    void syncRequested();

private:
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_syncButton = nullptr;
};
