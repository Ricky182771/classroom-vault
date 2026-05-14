#pragma once

#include "UiModels.hpp"

#include <QWidget>
#include <QVector>

class HeroStripWidget;
class KpiCardWidget;
class CourseGridWidget;
class ActivityPanelWidget;
class StorageCardWidget;
class QComboBox;
class QLabel;

class DashboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit DashboardWidget(QWidget *parent = nullptr);

    void setKpis(const QVector<KpiData> &kpis);
    void setCourses(const QVector<CourseUiState> &courses);
    void setActivity(const QVector<ActivityItem> &items);
    void setBasePath(const QString &basePath);
    void setStorageSummary(const QString &usedText, int attachments);
    void setLastSyncText(const QString &text);
    void setSearchText(const QString &text);

signals:
    void syncRequested();
    void openBasePathRequested();
    void openCourseRequested(const QString &courseId);
    void openFolderRequested(const QString &courseId);
    void syncCourseRequested(const QString &courseId);
    void openClassroomRequested(const QString &courseId);
    void showHistoryRequested();

private:
    HeroStripWidget *m_hero = nullptr;
    QVector<KpiCardWidget *> m_kpiCards;
    QLabel *m_coursesCountLabel = nullptr;
    QComboBox *m_filterCombo = nullptr;
    QComboBox *m_orderCombo = nullptr;
    CourseGridWidget *m_courseGrid = nullptr;
    ActivityPanelWidget *m_activityPanel = nullptr;
    StorageCardWidget *m_storageCard = nullptr;

    QVector<CourseUiState> m_courses;
};
