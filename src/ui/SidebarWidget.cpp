#include "SidebarWidget.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

struct NavItemDef {
    const char *id;
    const char *label;
    const char *icon;
};

const NavItemDef kMainItems[] = {
    {"home", "Inicio", "H"},
    {"courses", "Cursos", "C"},
    {"tasks", "Tareas", "T"},
    {"attachments", "Adjuntos", "A"},
    {"history", "Historial", "R"},
    {"settings", "Configuracion", "S"},
};

} // namespace

SidebarWidget::SidebarWidget(QWidget *parent)
    : QFrame(parent)
    , m_rootLayout(new QVBoxLayout(this))
{
    setObjectName(QStringLiteral("Sidebar"));
    setFixedWidth(240);
    m_rootLayout->setContentsMargins(10, 10, 10, 10);
    m_rootLayout->setSpacing(10);

    rebuildLayout();
}

void SidebarWidget::setCompact(bool compact)
{
    if (m_compact == compact) {
        return;
    }
    m_compact = compact;
    setFixedWidth(m_compact ? 72 : 240);
    rebuildLayout();
}

bool SidebarWidget::isCompact() const
{
    return m_compact;
}

void SidebarWidget::setCurrentPage(const QString &pageId)
{
    if (m_currentPage == pageId) {
        return;
    }
    m_currentPage = pageId;
    refreshActiveState();
}

QString SidebarWidget::currentPage() const
{
    return m_currentPage;
}

QPushButton *SidebarWidget::createNavButton(const QString &pageId, const QString &label, const QString &iconText)
{
    auto *button = new QPushButton(this);
    button->setProperty("nav", true);
    button->setProperty("pageId", pageId);
    button->setProperty("active", pageId == m_currentPage);
    button->setCursor(Qt::PointingHandCursor);

    if (m_compact) {
        button->setText(iconText);
        button->setToolTip(label);
    } else {
        button->setText(QStringLiteral("%1  %2").arg(iconText, label));
    }

    connect(button, &QPushButton::clicked, this, [this, pageId]() {
        if (m_currentPage != pageId) {
            m_currentPage = pageId;
            refreshActiveState();
        }
        emit navigationRequested(pageId);
    });

    return button;
}

void SidebarWidget::refreshActiveState()
{
    const auto buttons = findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        const QString pageId = button->property("pageId").toString();
        const bool isActive = (pageId == m_currentPage);
        button->setProperty("active", isActive);
        if (isActive) {
            button->setStyleSheet(
                QStringLiteral("background: rgba(91, 92, 246, 0.15);"
                               "border: 1px solid rgba(91, 92, 246, 0.45);"
                               "color: #F1F1F1;"));
        } else {
            button->setStyleSheet(QString());
        }
    }
}

void SidebarWidget::rebuildLayout()
{
    QLayoutItem *child;
    while ((child = m_rootLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    m_brandRow = new QFrame(this);
    m_brandRow->setObjectName(QStringLiteral("Section"));
    auto *brandLayout = new QVBoxLayout(m_brandRow);
    brandLayout->setContentsMargins(10, 10, 10, 10);
    brandLayout->setSpacing(2);

    auto *title = new QLabel(m_brandRow);
    title->setText(m_compact ? QStringLiteral("CV") : QStringLiteral("Classroom Vault"));
    title->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: 700; background: transparent; border: none;"));
    brandLayout->addWidget(title);

    if (!m_compact) {
        auto *subtitle = new QLabel(QStringLiteral("v0.x · TareaSync"), m_brandRow);
        subtitle->setProperty("subtle", true);
        subtitle->setStyleSheet(QStringLiteral("font-size: 11px; background: transparent; border: none;"));
        brandLayout->addWidget(subtitle);
    }

    m_rootLayout->addWidget(m_brandRow);

    auto *navContainer = new QFrame(this);
    navContainer->setObjectName(QStringLiteral("Section"));
    m_navLayout = new QVBoxLayout(navContainer);
    m_navLayout->setContentsMargins(8, 8, 8, 8);
    m_navLayout->setSpacing(6);

    for (const NavItemDef &item : kMainItems) {
        m_navLayout->addWidget(createNavButton(
            QString::fromUtf8(item.id),
            QString::fromUtf8(item.label),
            QString::fromUtf8(item.icon)));
    }

    m_navLayout->addStretch(1);
    m_rootLayout->addWidget(navContainer, 1);

    refreshActiveState();
}
