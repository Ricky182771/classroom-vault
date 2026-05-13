#include "TasksViewWidget.hpp"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
constexpr int CourseIdRole = Qt::UserRole + 101;
constexpr int AssignmentIdRole = Qt::UserRole + 102;
}

TasksViewWidget::TasksViewWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("Section"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    m_title = new QLabel(QStringLiteral("Tareas"), this);
    m_title->setStyleSheet(QStringLiteral("font-size:16px;font-weight:700;"));
    layout->addWidget(m_title);

    m_table = new QTableWidget(this);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("Materia"),
        QStringLiteral("Tarea"),
        QStringLiteral("Fecha de entrega"),
        QStringLiteral("Metadata"),
        QStringLiteral("Adjuntos"),
        QStringLiteral("Archivos"),
        QStringLiteral("Ultima sync"),
        QStringLiteral("Estado"),
    });

    auto *header = m_table->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(7, QHeaderView::ResizeToContents);

    layout->addWidget(m_table, 1);

    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        auto *courseItem = m_table->item(row, 0);
        if (!courseItem) {
            return;
        }
        emit taskActivated(
            courseItem->data(CourseIdRole).toString(),
            courseItem->data(AssignmentIdRole).toString());
    });
}

void TasksViewWidget::setRows(const QVector<TaskRowData> &rows)
{
    m_table->setRowCount(rows.size());

    for (int i = 0; i < rows.size(); ++i) {
        const TaskRowData &row = rows.at(i);

        auto *courseItem = new QTableWidgetItem(row.courseName);
        auto *titleItem = new QTableWidgetItem(row.title);
        auto *dueItem = new QTableWidgetItem(row.dueDate);
        auto *metadataItem = new QTableWidgetItem(row.metadataStatus);
        auto *attachmentsItem = new QTableWidgetItem(row.attachmentsStatus);
        auto *filesItem = new QTableWidgetItem(QString::number(row.files));
        auto *lastSyncItem = new QTableWidgetItem(row.lastSync);
        auto *stateItem = new QTableWidgetItem(row.state);

        courseItem->setData(CourseIdRole, row.courseId);
        courseItem->setData(AssignmentIdRole, row.assignmentId);

        m_table->setItem(i, 0, courseItem);
        m_table->setItem(i, 1, titleItem);
        m_table->setItem(i, 2, dueItem);
        m_table->setItem(i, 3, metadataItem);
        m_table->setItem(i, 4, attachmentsItem);
        m_table->setItem(i, 5, filesItem);
        m_table->setItem(i, 6, lastSyncItem);
        m_table->setItem(i, 7, stateItem);
    }
}
