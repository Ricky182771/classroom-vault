#include "AttachmentsViewWidget.hpp"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

AttachmentsViewWidget::AttachmentsViewWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("Section"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    m_title = new QLabel(QStringLiteral("Adjuntos"), this);
    m_title->setStyleSheet(QStringLiteral("font-size:16px;font-weight:700;background:transparent;border:none;"));
    layout->addWidget(m_title);

    m_table = new QTableWidget(this);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("Materia"),
        QStringLiteral("Tarea"),
        QStringLiteral("Archivo"),
        QStringLiteral("Tipo"),
        QStringLiteral("Estado"),
        QStringLiteral("Tamano"),
        QStringLiteral("Ruta local"),
    });

    auto *header = m_table->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::Stretch);
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(6, QHeaderView::Stretch);

    layout->addWidget(m_table, 1);
}

void AttachmentsViewWidget::setRows(const QVector<AttachmentRowData> &rows)
{
    m_table->setRowCount(rows.size());

    for (int i = 0; i < rows.size(); ++i) {
        const AttachmentRowData &row = rows.at(i);

        m_table->setItem(i, 0, new QTableWidgetItem(row.courseName));
        m_table->setItem(i, 1, new QTableWidgetItem(row.assignmentTitle));
        m_table->setItem(i, 2, new QTableWidgetItem(row.fileName));
        m_table->setItem(i, 3, new QTableWidgetItem(row.type));
        m_table->setItem(i, 4, new QTableWidgetItem(row.status));
        m_table->setItem(i, 5, new QTableWidgetItem(row.size));
        m_table->setItem(i, 6, new QTableWidgetItem(row.localPath));
    }
}
