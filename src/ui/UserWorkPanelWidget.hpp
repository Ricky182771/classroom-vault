#pragma once

#include <QFrame>
#include <QString>

class QListWidget;
class QLabel;
class QToolButton;

class UserWorkPanelWidget : public QFrame {
    Q_OBJECT

public:
    explicit UserWorkPanelWidget(QWidget *parent = nullptr);

    void setAssignmentFolderPath(const QString &path);
    QString assignmentFolderPath() const;

public slots:
    void refresh();

signals:
    void openAssignmentFolderRequested(const QString &path);
    void openUserFileRequested(const QString &path);

private:
    void rebuildList();

    QString m_assignmentFolderPath;
    QLabel *m_emptyLabel = nullptr;
    QListWidget *m_list = nullptr;
    QToolButton *m_openFolderButton = nullptr;
};
