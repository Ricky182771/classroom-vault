#pragma once

#include <QFrame>
#include <QString>

class QVBoxLayout;
class QPushButton;

class SidebarWidget : public QFrame {
    Q_OBJECT

public:
    explicit SidebarWidget(QWidget *parent = nullptr);

    void setCompact(bool compact);
    bool isCompact() const;

    void setCurrentPage(const QString &pageId);
    QString currentPage() const;

signals:
    void navigationRequested(const QString &pageId);

private:
    void rebuildLayout();
    QPushButton *createNavButton(const QString &pageId, const QString &label, const QString &iconText);
    void refreshActiveState();

    bool m_compact = false;
    QString m_currentPage = QStringLiteral("home");

    QVBoxLayout *m_rootLayout = nullptr;
    QFrame *m_brandRow = nullptr;
    QVBoxLayout *m_navLayout = nullptr;
};
