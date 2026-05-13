#pragma once

#include <QFrame>

class QHBoxLayout;
class QToolButton;
class QLabel;

class BreadcrumbWidget : public QFrame {
    Q_OBJECT

public:
    explicit BreadcrumbWidget(QWidget *parent = nullptr);

    void setHome();
    void setCourse(const QString &courseId, const QString &courseName);
    void setAssignment(
        const QString &courseId,
        const QString &courseName,
        const QString &assignmentId,
        const QString &assignmentTitle);

signals:
    void homeRequested();
    void courseRequested(const QString &courseId);

private:
    void clearTrail();
    QToolButton *addCrumbButton(const QString &text, const QString &courseId = QString());
    void addSeparator();

    QHBoxLayout *m_layout = nullptr;
    QString m_courseId;
};
