#include "SettingsViewWidget.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

SettingsViewWidget::SettingsViewWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("Section"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Configuracion"), this);
    title->setStyleSheet(QStringLiteral("font-size:16px;font-weight:700;background:transparent;border:none;"));
    root->addWidget(title);

    auto *generalGroup = new QGroupBox(QStringLiteral("General"), this);
    auto *generalLayout = new QFormLayout(generalGroup);

    auto *pathRow = new QWidget(generalGroup);
    auto *pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->setSpacing(6);
    m_basePathEdit = new QLineEdit(pathRow);
    m_basePathEdit->setReadOnly(true);
    m_changePathButton = new QPushButton(QStringLiteral("Cambiar ruta"), pathRow);
    pathLayout->addWidget(m_basePathEdit, 1);
    pathLayout->addWidget(m_changePathButton);

    generalLayout->addRow(QStringLiteral("Ruta base"), pathRow);

    m_autoDownloadCheck = new QCheckBox(QStringLiteral("Descargar adjuntos al sincronizar"), generalGroup);
    generalLayout->addRow(QString(), m_autoDownloadCheck);

    auto *skipUnchanged = new QCheckBox(QStringLiteral("Omitir archivos sin cambios"), generalGroup);
    skipUnchanged->setChecked(true);
    skipUnchanged->setEnabled(false);
    generalLayout->addRow(QString(), skipUnchanged);

    auto *replaceModified = new QCheckBox(QStringLiteral("Reemplazar archivos modificados"), generalGroup);
    replaceModified->setChecked(true);
    replaceModified->setEnabled(false);
    generalLayout->addRow(QString(), replaceModified);

    root->addWidget(generalGroup);

    auto *exportGroup = new QGroupBox(QStringLiteral("Exportacion"), this);
    auto *exportLayout = new QVBoxLayout(exportGroup);
    exportLayout->setSpacing(4);

    auto *docsLabel = new QLabel(QStringLiteral("Docs → DOCX · Sheets → XLSX · Slides → PPTX · Drawings → PNG"), exportGroup);
    docsLabel->setProperty("muted", true);
    docsLabel->setWordWrap(true);
    exportLayout->addWidget(docsLabel);

    auto *linksLabel = new QLabel(QStringLiteral("Forms, links y YouTube se guardan como .url"), exportGroup);
    linksLabel->setProperty("muted", true);
    exportLayout->addWidget(linksLabel);

    root->addWidget(exportGroup);

    auto *accountGroup = new QGroupBox(QStringLiteral("Cuenta"), this);
    auto *accountLayout = new QFormLayout(accountGroup);
    m_emailLabel = new QLabel(QStringLiteral("—"), accountGroup);
    m_tokenStatusLabel = new QLabel(QStringLiteral("No conectado"), accountGroup);
    accountLayout->addRow(QStringLiteral("Cuenta"), m_emailLabel);
    accountLayout->addRow(QStringLiteral("Estado"), m_tokenStatusLabel);

    auto *accountButtons = new QWidget(accountGroup);
    auto *accountButtonsLayout = new QHBoxLayout(accountButtons);
    accountButtonsLayout->setContentsMargins(0, 0, 0, 0);
    accountButtonsLayout->setSpacing(6);

    m_loginButton = new QPushButton(QStringLiteral("Iniciar sesion"), accountButtons);
    m_logoutButton = new QPushButton(QStringLiteral("Cerrar sesion"), accountButtons);
    m_loadClassroomButton = new QPushButton(QStringLiteral("Cargar Classroom"), accountButtons);
    m_loadSampleButton = new QPushButton(QStringLiteral("Cargar muestra"), accountButtons);

    m_loginButton->setProperty("variant", QStringLiteral("primary"));

    accountButtonsLayout->addWidget(m_loginButton);
    accountButtonsLayout->addWidget(m_logoutButton);
    accountButtonsLayout->addWidget(m_loadClassroomButton);
    accountButtonsLayout->addWidget(m_loadSampleButton);

    accountLayout->addRow(QString(), accountButtons);

    root->addWidget(accountGroup);
    root->addStretch(1);

    connect(m_changePathButton, &QPushButton::clicked, this, &SettingsViewWidget::changeBasePathRequested);
    connect(m_autoDownloadCheck, &QCheckBox::toggled, this, &SettingsViewWidget::autoDownloadAttachmentsChanged);
    connect(m_loginButton, &QPushButton::clicked, this, &SettingsViewWidget::loginRequested);
    connect(m_logoutButton, &QPushButton::clicked, this, &SettingsViewWidget::logoutRequested);
    connect(m_loadClassroomButton, &QPushButton::clicked, this, &SettingsViewWidget::loadClassroomRequested);
    connect(m_loadSampleButton, &QPushButton::clicked, this, &SettingsViewWidget::loadSampleRequested);
}

void SettingsViewWidget::setBasePath(const QString &path)
{
    m_basePathEdit->setText(path.trimmed());
}

void SettingsViewWidget::setConnectedEmail(const QString &email)
{
    m_emailLabel->setText(email.trimmed().isEmpty() ? QStringLiteral("—") : email.trimmed());
}

void SettingsViewWidget::setTokenStatus(const QString &status)
{
    m_tokenStatusLabel->setText(status.trimmed().isEmpty() ? QStringLiteral("No conectado") : status.trimmed());
}

void SettingsViewWidget::setAutoDownloadAttachments(bool enabled)
{
    if (m_autoDownloadCheck->isChecked() == enabled) {
        return;
    }
    m_autoDownloadCheck->setChecked(enabled);
}
