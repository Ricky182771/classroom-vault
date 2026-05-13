#include "TopBarWidget.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

TopBarWidget::TopBarWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("TopBar"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    m_toggleSidebarButton = new QPushButton(QStringLiteral("≡"), this);
    m_toggleSidebarButton->setProperty("variant", QStringLiteral("ghost"));
    m_toggleSidebarButton->setFixedWidth(34);
    m_toggleSidebarButton->setToolTip(QStringLiteral("Alternar barra lateral"));
    layout->addWidget(m_toggleSidebarButton);

    m_breadcrumbLeft = new QLabel(QStringLiteral("Inicio"), this);
    m_breadcrumbLeft->setProperty("subtle", true);
    m_breadcrumbRight = new QLabel(QStringLiteral("Resumen de respaldo"), this);
    m_breadcrumbRight->setStyleSheet(QStringLiteral("font-weight: 600;"));

    layout->addWidget(m_breadcrumbLeft);
    auto *arrow = new QLabel(QStringLiteral("›"), this);
    arrow->setProperty("subtle", true);
    layout->addWidget(arrow);
    layout->addWidget(m_breadcrumbRight);

    layout->addSpacing(12);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("Buscar materias, tareas o archivos…"));
    m_searchEdit->setMinimumWidth(260);
    layout->addWidget(m_searchEdit, 1);

    auto *stateTag = new QFrame(this);
    stateTag->setObjectName(QStringLiteral("Section"));
    auto *stateLayout = new QHBoxLayout(stateTag);
    stateLayout->setContentsMargins(8, 5, 8, 5);
    stateLayout->setSpacing(6);

    auto *dot = new QLabel(QStringLiteral("●"), stateTag);
    dot->setStyleSheet(QStringLiteral("color:#8FD19E;font-size:11px;"));
    stateLayout->addWidget(dot);

    m_connectionLabel = new QLabel(QStringLiteral("Conectado"), stateTag);
    stateLayout->addWidget(m_connectionLabel);

    m_emailLabel = new QLabel(QStringLiteral("—"), stateTag);
    m_emailLabel->setProperty("subtle", true);
    stateLayout->addWidget(m_emailLabel);

    layout->addWidget(stateTag);

    m_syncButton = new QPushButton(QStringLiteral("Sincronizar"), this);
    m_syncButton->setProperty("variant", QStringLiteral("primary"));
    layout->addWidget(m_syncButton);

    m_settingsButton = new QPushButton(QStringLiteral("⚙"), this);
    m_settingsButton->setProperty("variant", QStringLiteral("ghost"));
    m_settingsButton->setFixedWidth(36);
    m_settingsButton->setToolTip(QStringLiteral("Configuracion"));
    layout->addWidget(m_settingsButton);

    auto *avatar = new QLabel(QStringLiteral("R"), this);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(30, 30);
    avatar->setObjectName(QStringLiteral("Section"));
    avatar->setStyleSheet(QStringLiteral("border-radius:15px;font-weight:700;"));
    layout->addWidget(avatar);

    connect(m_toggleSidebarButton, &QPushButton::clicked, this, &TopBarWidget::toggleSidebarRequested);
    connect(m_syncButton, &QPushButton::clicked, this, &TopBarWidget::syncRequested);
    connect(m_settingsButton, &QPushButton::clicked, this, &TopBarWidget::settingsRequested);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        emit searchRequested(m_searchEdit->text().trimmed());
    });
}

void TopBarWidget::setBreadcrumb(const QString &left, const QString &right)
{
    m_breadcrumbLeft->setText(left);
    m_breadcrumbRight->setText(right);
}

void TopBarWidget::setConnectionState(const QString &state)
{
    m_connectionLabel->setText(state);
}

void TopBarWidget::setConnectedEmail(const QString &email)
{
    m_emailLabel->setText(email.trimmed().isEmpty() ? QStringLiteral("—") : email.trimmed());
}
