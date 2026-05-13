#include "BreadcrumbWidget.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

BreadcrumbWidget::BreadcrumbWidget(QWidget *parent)
    : QFrame(parent)
    , m_layout(new QHBoxLayout(this))
{
    setObjectName(QStringLiteral("BreadcrumbBar"));

    m_layout->setContentsMargins(12, 8, 12, 8);
    m_layout->setSpacing(4);
    setHome();
}

void BreadcrumbWidget::setHome()
{
    clearTrail();

    auto *homeButton = addCrumbButton(QStringLiteral("Inicio"));
    connect(homeButton, &QToolButton::clicked, this, &BreadcrumbWidget::homeRequested);
    m_layout->addStretch(1);

    m_courseId.clear();
}

void BreadcrumbWidget::setCourse(const QString &courseId, const QString &courseName)
{
    clearTrail();

    auto *homeButton = addCrumbButton(QStringLiteral("Inicio"));
    connect(homeButton, &QToolButton::clicked, this, &BreadcrumbWidget::homeRequested);

    addSeparator();

    auto *courseButton = addCrumbButton(courseName.trimmed().isEmpty() ? QStringLiteral("Materia") : courseName.trimmed(), courseId);
    courseButton->setEnabled(false);

    m_layout->addStretch(1);
    m_courseId = courseId.trimmed();
}

void BreadcrumbWidget::setAssignment(
    const QString &courseId,
    const QString &courseName,
    const QString &assignmentId,
    const QString &assignmentTitle)
{
    Q_UNUSED(assignmentId)

    clearTrail();

    auto *homeButton = addCrumbButton(QStringLiteral("Inicio"));
    connect(homeButton, &QToolButton::clicked, this, &BreadcrumbWidget::homeRequested);

    addSeparator();

    auto *courseButton = addCrumbButton(courseName.trimmed().isEmpty() ? QStringLiteral("Materia") : courseName.trimmed(), courseId);
    connect(courseButton, &QToolButton::clicked, this, [this, courseId]() {
        emit courseRequested(courseId);
    });

    addSeparator();

    auto *assignmentButton = addCrumbButton(assignmentTitle.trimmed().isEmpty() ? QStringLiteral("Tarea") : assignmentTitle.trimmed());
    assignmentButton->setEnabled(false);

    m_layout->addStretch(1);
    m_courseId = courseId.trimmed();
}

void BreadcrumbWidget::clearTrail()
{
    QLayoutItem *item = nullptr;
    while ((item = m_layout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

QToolButton *BreadcrumbWidget::addCrumbButton(const QString &text, const QString &courseId)
{
    auto *button = new QToolButton(this);
    button->setText(text);
    button->setProperty("variant", QStringLiteral("ghost"));
    button->setAutoRaise(true);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    if (!courseId.trimmed().isEmpty()) {
        button->setProperty("courseId", courseId.trimmed());
    }
    m_layout->addWidget(button);
    return button;
}

void BreadcrumbWidget::addSeparator()
{
    auto *separator = new QLabel(QStringLiteral("›"), this);
    separator->setProperty("subtle", true);
    m_layout->addWidget(separator);
}
