#include "StyleManager.hpp"

#include <QApplication>
#include <QPalette>

namespace StyleManager {

QString darkStyleSheet()
{
    return QStringLiteral(R"QSS(
* {
    color: #F1F1F1;
    selection-background-color: rgba(91, 92, 246, 0.35);
    selection-color: #F1F1F1;
}
QMainWindow,
QWidget#AppRoot {
    background: #1E1F24;
}
QWidget {
    background: transparent;
    color: #F1F1F1;
    font-size: 13px;
}
QLabel {
    background: transparent;
    border: 0;
}
QFrame#Sidebar,
QFrame#TopBar,
QFrame#PathBar,
QFrame#BreadcrumbBar,
QFrame#ActivityDrawer,
QFrame#StatusBar {
    background: #232429;
    border: 1px solid #2F313A;
}
QFrame#TopBar,
QFrame#PathBar,
QFrame#BreadcrumbBar,
QFrame#StatusBar {
    border-left: 0;
    border-right: 0;
}
QFrame#Card,
QFrame#HeroCard,
QFrame#StorageCard,
QFrame#ActivityPanel,
QFrame#KpiCard,
QFrame#CourseCard,
QFrame#AssignmentItem,
QFrame#AttachmentCard,
QFrame#Section {
    background: #2A2C33;
    border: 1px solid #3A3D46;
    border-radius: 12px;
}
QLabel#Section {
    background: #30323A;
    border: 1px solid #3A3D46;
    border-radius: 8px;
    padding: 3px 8px;
}
QFrame#CourseBanner {
    border-top-left-radius: 11px;
    border-top-right-radius: 11px;
    border-bottom-left-radius: 0;
    border-bottom-right-radius: 0;
    border: 0;
}
QPushButton,
QToolButton {
    background: #30323A;
    border: 1px solid #3A3D46;
    border-radius: 8px;
    padding: 6px 10px;
    color: #F1F1F1;
}
QPushButton:hover,
QToolButton:hover {
    border-color: #5B5CF6;
}
QPushButton:pressed,
QToolButton:pressed {
    background: #2B2D35;
}
QPushButton:disabled,
QToolButton:disabled {
    color: #7C808C;
    border-color: #2F313A;
    background: #252730;
}
QPushButton[variant="primary"],
QToolButton[variant="primary"] {
    background: #5B5CF6;
    border-color: #5B5CF6;
    color: #FFFFFF;
    font-weight: 600;
}
QPushButton[variant="ghost"],
QToolButton[variant="ghost"] {
    background: transparent;
}
QPushButton[nav="true"] {
    text-align: left;
    padding: 8px 10px;
    border: 1px solid transparent;
    background: transparent;
    border-radius: 10px;
    color: #B8BCC7;
}
QPushButton[nav="true"][active="true"] {
    background: rgba(91, 92, 246, 0.15);
    border-color: rgba(91, 92, 246, 0.45);
    color: #F1F1F1;
}
QLineEdit,
QPlainTextEdit,
QTextEdit,
QComboBox,
QSpinBox {
    background: #30323A;
    border: 1px solid #3A3D46;
    border-radius: 8px;
    padding: 6px 8px;
    color: #F1F1F1;
}
QLineEdit:focus,
QPlainTextEdit:focus,
QTextEdit:focus,
QComboBox:focus {
    border-color: #5B5CF6;
}
QComboBox::drop-down {
    border: 0;
    width: 20px;
}
QComboBox::down-arrow {
    image: none;
    border: 0;
}
QHeaderView::section {
    background: #232429;
    color: #B8BCC7;
    border: 0;
    border-bottom: 1px solid #3A3D46;
    padding: 8px;
    font-weight: 600;
}
QTableWidget,
QTableView,
QListWidget {
    background: #2A2C33;
    alternate-background-color: #30323A;
    border: 1px solid #3A3D46;
    border-radius: 10px;
    gridline-color: #343740;
}
QTableWidget::item,
QTableView::item,
QListWidget::item {
    padding: 6px;
}
QTableWidget::item:selected,
QTableView::item:selected,
QListWidget::item:selected {
    background: rgba(91, 92, 246, 0.22);
}
QProgressBar {
    background: #232429;
    border: 1px solid #3A3D46;
    border-radius: 7px;
    min-height: 12px;
    text-align: center;
}
QProgressBar::chunk {
    border-radius: 6px;
    background: #5B5CF6;
}
QScrollArea {
    border: 0;
}
QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 2px;
}
QScrollBar::handle:vertical {
    background: #3A3D46;
    min-height: 24px;
    border-radius: 5px;
}
QScrollBar::handle:vertical:hover {
    background: #4B4F5A;
}
QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical,
QScrollBar::sub-page:vertical {
    background: transparent;
    border: 0;
    height: 0;
}
QLabel[muted="true"] {
    color: #B8BCC7;
}
QLabel[subtle="true"] {
    color: #7C808C;
}
QLabel[status="complete"] {
    color: #8FD19E;
}
QLabel[status="pending"] {
    color: #E6C26A;
}
QLabel[status="error"] {
    color: #E07C7C;
}
QLabel[status="idle"] {
    color: #9AA0AE;
}
)QSS");
}

void applyDarkTheme(QApplication &app)
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#1E1F24")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#F1F1F1")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#2A2C33")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#30323A")));
    palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#2A2C33")));
    palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#F1F1F1")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#F1F1F1")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#30323A")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#F1F1F1")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#5B5CF6")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#FFFFFF")));
    app.setPalette(palette);
    app.setStyleSheet(darkStyleSheet());
}

} // namespace StyleManager
