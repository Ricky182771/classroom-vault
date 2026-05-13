#include "HeroStripWidget.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

HeroStripWidget::HeroStripWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("HeroCard"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *logo = new QLabel(QStringLiteral("CV"), this);
    logo->setAlignment(Qt::AlignCenter);
    logo->setFixedSize(42, 42);
    logo->setStyleSheet(
        QStringLiteral("background: rgba(91,92,246,0.2); border:1px solid #5B5CF6; border-radius:10px; font-weight:700;"));
    layout->addWidget(logo);

    auto *textCol = new QVBoxLayout();
    textCol->setSpacing(4);

    auto *title = new QLabel(QStringLiteral("Tu archivo historico de Classroom"), this);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 700; background: transparent; border: none;"));
    textCol->addWidget(title);

    auto *subtitle = new QLabel(
        QStringLiteral("Classroom Vault convierte Google Classroom en un archivo local, ordenado y verificable."),
        this);
    subtitle->setWordWrap(true);
    subtitle->setProperty("muted", true);
    textCol->addWidget(subtitle);

    layout->addLayout(textCol, 1);

    auto *rightCol = new QVBoxLayout();
    rightCol->setSpacing(8);
    rightCol->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_statusLabel = new QLabel(QStringLiteral("API estable · token OK"), this);
    m_statusLabel->setObjectName(QStringLiteral("Section"));
    m_statusLabel->setStyleSheet(
        QStringLiteral("padding:6px 10px;border-radius:8px;background:#30323A;border:1px solid #3A3D46;"));
    rightCol->addWidget(m_statusLabel, 0, Qt::AlignRight);

    m_syncButton = new QPushButton(QStringLiteral("Sincronizar"), this);
    m_syncButton->setProperty("variant", QStringLiteral("primary"));
    rightCol->addWidget(m_syncButton, 0, Qt::AlignRight);

    layout->addLayout(rightCol);

    connect(m_syncButton, &QPushButton::clicked, this, &HeroStripWidget::syncRequested);
}

void HeroStripWidget::setApiStatusText(const QString &text)
{
    m_statusLabel->setText(text.trimmed().isEmpty() ? QStringLiteral("API estable") : text.trimmed());
}
