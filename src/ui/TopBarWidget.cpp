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

    auto *appLabel = new QLabel(QStringLiteral("Classroom Vault"), this);
    appLabel->setStyleSheet(QStringLiteral("font-size:15px;font-weight:700;background:transparent;border:none;"));
    layout->addWidget(appLabel);

    m_titleLabel = new QLabel(QStringLiteral("Inicio"), this);
    m_titleLabel->setProperty("subtle", true);
    layout->addWidget(m_titleLabel);

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
    dot->setStyleSheet(QStringLiteral("color:#8FD19E;font-size:11px;background:transparent;border:none;"));
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

    m_accountButton = new QPushButton(QStringLiteral("Cuenta"), this);
    m_accountButton->setProperty("variant", QStringLiteral("ghost"));
    layout->addWidget(m_accountButton);

    auto *avatar = new QLabel(QStringLiteral("R"), this);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(30, 30);
    avatar->setObjectName(QStringLiteral("Section"));
    avatar->setStyleSheet(QStringLiteral("border-radius:15px;font-weight:700;background:#30323A;border:1px solid #3A3D46;"));
    layout->addWidget(avatar);

    connect(m_syncButton, &QPushButton::clicked, this, &TopBarWidget::syncRequested);
    connect(m_accountButton, &QPushButton::clicked, this, &TopBarWidget::accountRequested);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        emit searchRequested(m_searchEdit->text().trimmed());
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit searchTextChanged(text.trimmed());
    });
}

void TopBarWidget::setPageTitle(const QString &title)
{
    m_titleLabel->setText(title.trimmed().isEmpty() ? QStringLiteral("Inicio") : title.trimmed());
}

void TopBarWidget::setConnectionState(const QString &state)
{
    m_connectionLabel->setText(state);
}

void TopBarWidget::setConnectedEmail(const QString &email)
{
    m_emailLabel->setText(email.trimmed().isEmpty() ? QStringLiteral("—") : email.trimmed());
}

void TopBarWidget::setSearchPlaceholder(const QString &placeholder)
{
    m_searchEdit->setPlaceholderText(
        placeholder.trimmed().isEmpty() ? QStringLiteral("Buscar materias, tareas o archivos…") : placeholder.trimmed());
}
