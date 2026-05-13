#include "HistoryViewWidget.hpp"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

HistoryViewWidget::HistoryViewWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("Section"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("Historial"), this);
    title->setStyleSheet(QStringLiteral("font-size:16px;font-weight:700;background:transparent;border:none;"));
    layout->addWidget(title);

    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(8);

    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem(QStringLiteral("Todos"), QStringLiteral("all"));
    m_filterCombo->addItem(QStringLiteral("Sync"), QStringLiteral("sync"));
    m_filterCombo->addItem(QStringLiteral("Descargas"), QStringLiteral("downloads"));
    m_filterCombo->addItem(QStringLiteral("Exportaciones"), QStringLiteral("exports"));
    m_filterCombo->addItem(QStringLiteral("Errores"), QStringLiteral("errors"));
    toolbar->addWidget(m_filterCombo);

    toolbar->addStretch(1);

    m_clearButton = new QPushButton(QStringLiteral("Limpiar logs"), this);
    m_copyErrorsButton = new QPushButton(QStringLiteral("Copiar errores"), this);
    m_saveLogButton = new QPushButton(QStringLiteral("Guardar log"), this);
    m_saveLogButton->setProperty("variant", QStringLiteral("ghost"));

    toolbar->addWidget(m_clearButton);
    toolbar->addWidget(m_copyErrorsButton);
    toolbar->addWidget(m_saveLogButton);
    layout->addLayout(toolbar);

    m_logEdit = new QPlainTextEdit(this);
    m_logEdit->setReadOnly(true);
    layout->addWidget(m_logEdit, 1);

    connect(m_filterCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        refreshView();
    });

    connect(m_clearButton, &QPushButton::clicked, this, [this]() {
        emit clearLogsRequested();
    });

    connect(m_copyErrorsButton, &QPushButton::clicked, this, [this]() {
        const QStringList lines = filteredLines();
        QStringList errorLines;
        for (const QString &line : lines) {
            const QString upper = line.toUpper();
            if (upper.contains(QStringLiteral(" ERR "))
                || upper.contains(QStringLiteral(" ERROR "))
                || upper.contains(QStringLiteral("ERR   "))) {
                errorLines.append(line);
            }
        }

        if (errorLines.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("Historial"), QStringLiteral("No hay errores para copiar."));
            return;
        }

        QApplication::clipboard()->setText(errorLines.join('\n'));
    });

    connect(m_saveLogButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("Guardar log"),
            QStringLiteral("classroom-vault-log.txt"),
            QStringLiteral("Text Files (*.txt);;All Files (*)"));
        if (path.trimmed().isEmpty()) {
            return;
        }

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QMessageBox::warning(this, QStringLiteral("Historial"), QStringLiteral("No se pudo guardar el archivo."));
            return;
        }

        const QString joined = filteredLines().join('\n') + '\n';
        file.write(joined.toUtf8());
        file.close();
    });
}

void HistoryViewWidget::setLogLines(const QStringList &lines)
{
    m_allLines = lines;
    refreshView();
}

QStringList HistoryViewWidget::filteredLines() const
{
    const QString mode = m_filterCombo->currentData().toString();
    if (mode == QStringLiteral("all")) {
        return m_allLines;
    }

    QStringList filtered;
    filtered.reserve(m_allLines.size());

    for (const QString &line : m_allLines) {
        const QString upper = line.toUpper();

        if (mode == QStringLiteral("errors")) {
            if (upper.contains(QStringLiteral(" ERR ")) || upper.contains(QStringLiteral(" ERROR ")) || upper.contains(QStringLiteral("ERR   "))) {
                filtered.append(line);
            }
            continue;
        }

        if (mode == QStringLiteral("sync")) {
            if (upper.contains(QStringLiteral("SYNC")) || upper.contains(QStringLiteral("NEW"))
                || upper.contains(QStringLiteral("UPD")) || upper.contains(QStringLiteral("SAME"))) {
                filtered.append(line);
            }
            continue;
        }

        if (mode == QStringLiteral("downloads")) {
            if (upper.contains(QStringLiteral("FILE")) || upper.contains(QStringLiteral("SAVE"))
                || upper.contains(QStringLiteral("SKIP")) || upper.contains(QStringLiteral("LINK"))
                || upper.contains(QStringLiteral("FORM")) || upper.contains(QStringLiteral("YT"))) {
                filtered.append(line);
            }
            continue;
        }

        if (mode == QStringLiteral("exports")) {
            if (upper.contains(QStringLiteral("GDOC")) || upper.contains(QStringLiteral("GSHT"))
                || upper.contains(QStringLiteral("GSLD")) || upper.contains(QStringLiteral("GDRW"))
                || upper.contains(QStringLiteral("EXPORT"))) {
                filtered.append(line);
            }
            continue;
        }
    }

    return filtered;
}

void HistoryViewWidget::refreshView()
{
    m_logEdit->setPlainText(filteredLines().join('\n'));
}
