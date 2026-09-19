#include "UserWorkPanelWidget.hpp"

#include "../FolderOrganizer.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

UserWorkPanelWidget::UserWorkPanelWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("Section"));
    setMinimumWidth(250);
    setMaximumWidth(300);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto *header = new QHBoxLayout();
    auto *title = new QLabel(QStringLiteral("Tu trabajo"), this);
    title->setStyleSheet(QStringLiteral("font-size:15px;font-weight:700;background:transparent;border:none;"));
    header->addWidget(title);
    header->addStretch(1);

    m_openFolderButton = new QToolButton(this);
    m_openFolderButton->setText(QStringLiteral("[DIR]"));
    m_openFolderButton->setToolTip(QStringLiteral("Abrir carpeta de la tarea"));
    m_openFolderButton->setFixedSize(28, 28);
    header->addWidget(m_openFolderButton);
    root->addLayout(header);

    m_emptyLabel = new QLabel(QStringLiteral("Aun no hay archivos propios en esta tarea."), this);
    m_emptyLabel->setProperty("subtle", true);
    m_emptyLabel->setWordWrap(true);
    root->addWidget(m_emptyLabel);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setAlternatingRowColors(false);
    m_list->setUniformItemSizes(false);
    m_list->setWordWrap(false);
    root->addWidget(m_list, 1);

    connect(m_openFolderButton, &QToolButton::clicked, this, [this]() {
        if (m_assignmentFolderPath.trimmed().isEmpty()) {
            QMessageBox::information(this, QStringLiteral("Tu trabajo"), QStringLiteral("La tarea no tiene carpeta local."));
            return;
        }

        emit openAssignmentFolderRequested(m_assignmentFolderPath);
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_assignmentFolderPath));
    });

    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (!item) {
            return;
        }
        const QString path = item->data(Qt::UserRole).toString();
        if (path.trimmed().isEmpty()) {
            return;
        }

        emit openUserFileRequested(path);
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });

    rebuildList();
}

void UserWorkPanelWidget::setAssignmentFolderPath(const QString &path)
{
    const QString clean = path.trimmed();
    if (m_assignmentFolderPath == clean) {
        return;
    }

    m_assignmentFolderPath = clean;
    rebuildList();
}

QString UserWorkPanelWidget::assignmentFolderPath() const
{
    return m_assignmentFolderPath;
}

void UserWorkPanelWidget::refresh()
{
    rebuildList();
}

void UserWorkPanelWidget::rebuildList()
{
    m_list->clear();

    const QString folderPath = m_assignmentFolderPath.trimmed();
    if (folderPath.isEmpty()) {
        m_emptyLabel->setText(QStringLiteral("Aun no hay carpeta local para esta tarea."));
        m_emptyLabel->show();
        m_list->hide();
        return;
    }

    const QDir dir(folderPath);
    if (!dir.exists()) {
        m_emptyLabel->setText(QStringLiteral("La carpeta local de la tarea no existe."));
        m_emptyLabel->show();
        m_list->hide();
        return;
    }

    const QFileInfoList entries = dir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    for (const QFileInfo &entry : entries) {
        const QString name = entry.fileName();
        if (name.compare(QStringLiteral("Adjuntos"), Qt::CaseInsensitive) == 0) {
            continue;
        }
        if (name.compare(QStringLiteral("metadata.json"), Qt::CaseInsensitive) == 0) {
            continue;
        }
        if (name.compare(QStringLiteral("descripcion.md"), Qt::CaseInsensitive) == 0) {
            continue;
        }

        const QString iconPrefix = entry.isDir() ? QStringLiteral("[DIR] ") : QStringLiteral("[FILE] ");
        auto *item = new QListWidgetItem(iconPrefix + FolderOrganizer::sanitizeFileName(name), m_list);
        item->setData(Qt::UserRole, entry.absoluteFilePath());
        m_list->addItem(item);
    }

    if (m_list->count() == 0) {
        m_emptyLabel->setText(QStringLiteral("Aun no hay archivos propios en esta tarea."));
        m_emptyLabel->show();
        m_list->hide();
    } else {
        m_emptyLabel->hide();
        m_list->show();
    }
}
