#include "StatusBarWidget.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>

StatusBarWidget::StatusBarWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("StatusBar"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(10);

    m_statusLabel = new QLabel(QStringLiteral("Listo"), this);
    layout->addWidget(m_statusLabel);

    m_lastActionLabel = new QLabel(QStringLiteral("Sin actividad reciente"), this);
    m_lastActionLabel->setProperty("subtle", true);
    layout->addWidget(m_lastActionLabel, 1);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 1);
    m_progressBar->setValue(0);
    m_progressBar->setFixedWidth(180);
    m_progressBar->setFormat(QStringLiteral("Sin progreso"));
    layout->addWidget(m_progressBar);

    m_errorLabel = new QLabel(QStringLiteral("Errores: 0"), this);
    m_errorLabel->setProperty("subtle", true);
    layout->addWidget(m_errorLabel);
}

void StatusBarWidget::setStatusText(const QString &text)
{
    m_statusLabel->setText(text.trimmed().isEmpty() ? QStringLiteral("Listo") : text.trimmed());
}

void StatusBarWidget::setLastActionText(const QString &text)
{
    m_lastActionLabel->setText(text.trimmed().isEmpty() ? QStringLiteral("Sin actividad reciente") : text.trimmed());
}

void StatusBarWidget::setErrorCount(int errors)
{
    const int safeErrors = errors < 0 ? 0 : errors;
    m_errorLabel->setText(QStringLiteral("Errores: %1").arg(safeErrors));
}

void StatusBarWidget::setProgress(int current, int total)
{
    if (total <= 0) {
        m_progressBar->setRange(0, 1);
        m_progressBar->setValue(0);
        m_progressBar->setFormat(QStringLiteral("Sin progreso"));
        return;
    }

    if (current < 0) {
        current = 0;
    }
    if (current > total) {
        current = total;
    }

    m_progressBar->setRange(0, total);
    m_progressBar->setValue(current);
    m_progressBar->setFormat(QStringLiteral("%1/%2").arg(current).arg(total));
}
