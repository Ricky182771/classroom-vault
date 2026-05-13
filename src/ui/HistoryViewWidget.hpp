#pragma once

#include <QFrame>
#include <QStringList>

class QComboBox;
class QPlainTextEdit;
class QPushButton;

class HistoryViewWidget : public QFrame {
    Q_OBJECT

public:
    explicit HistoryViewWidget(QWidget *parent = nullptr);

    void setLogLines(const QStringList &lines);

signals:
    void clearLogsRequested();

private:
    QStringList filteredLines() const;
    void refreshView();

    QComboBox *m_filterCombo = nullptr;
    QPushButton *m_clearButton = nullptr;
    QPushButton *m_copyErrorsButton = nullptr;
    QPushButton *m_saveLogButton = nullptr;
    QPlainTextEdit *m_logEdit = nullptr;

    QStringList m_allLines;
};
