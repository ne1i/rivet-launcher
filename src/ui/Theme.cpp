#include "Theme.h"

#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QStyleFactory>

namespace Theme {

QString styleSheet()
{
    return QStringLiteral(R"(
* {
    outline: none;
}

QWidget {
    color: #f4f4f7;
    font-size: 10pt;
}

QMainWindow, QDialog {
    background-color: #101014;
}

QWidget#page {
    background-color: #101014;
}

QFrame#topBar {
    background-color: #101014;
    border-bottom: 1px solid #23232c;
}

QFrame#surface, QWidget#surface {
    background-color: #1d1d25;
    border: 1px solid #2e2e3a;
    border-radius: 12px;
}

QLabel#appTitle {
    font-size: 15pt;
    font-weight: 700;
    color: #f4f4f7;
}

QLabel#appSubtitle {
    color: #9a9aab;
    font-size: 9pt;
}

QLabel#pageTitle {
    font-size: 24pt;
    font-weight: 700;
}

QLabel#sectionTitle {
    font-size: 13pt;
    font-weight: 700;
}

QLabel#cardTitle {
    font-size: 11pt;
    font-weight: 600;
}

QLabel#muted {
    color: #9a9aab;
}

QLabel#small {
    color: #9a9aab;
    font-size: 9pt;
}

QLabel#error {
    color: #ff7075;
}

QLabel#success {
    color: #67d99a;
}

QLabel#badge {
    background-color: #2a2a34;
    border: 1px solid #343440;
    border-radius: 9px;
    padding: 3px 9px;
    color: #cfcfda;
    font-size: 9pt;
}

QPushButton {
    background-color: #24242d;
    border: 1px solid #31313d;
    border-radius: 8px;
    padding: 7px 14px;
    color: #f4f4f7;
}

QPushButton:hover {
    background-color: #2c2c37;
    border-color: #3b3b49;
}

QPushButton:pressed {
    background-color: #202028;
}

QPushButton:focus {
    border-color: #f2a93b;
}

QPushButton:disabled {
    color: #666677;
    background-color: #191920;
    border-color: #24242d;
}

QPushButton#primary {
    background-color: #f2a93b;
    border: 1px solid #f2a93b;
    color: #1a1206;
    font-weight: 700;
    padding: 8px 18px;
}

QPushButton#primary:hover {
    background-color: #ffbe55;
    border-color: #ffbe55;
}

QPushButton#primary:disabled {
    background-color: #3a3324;
    border-color: #3a3324;
    color: #8a7b5c;
}

QPushButton#danger {
    background-color: #3a2023;
    border-color: #5a2a2f;
    color: #ff9b9f;
}

QPushButton#danger:hover {
    background-color: #4a252a;
    border-color: #7a343c;
}

QPushButton#ghost {
    background-color: transparent;
    border: none;
    padding: 6px;
    color: #c9c9d4;
}

QPushButton#ghost:hover {
    background-color: #24242d;
}

QPushButton#link {
    background: transparent;
    border: none;
    color: #f2a93b;
    padding: 2px 0;
    text-align: left;
}

QPushButton#link:hover {
    color: #ffbe55;
    text-decoration: underline;
}

QLineEdit, QComboBox, QSpinBox, QPlainTextEdit, QTextEdit {
    background-color: #17171e;
    border: 1px solid #31313d;
    border-radius: 8px;
    padding: 7px 10px;
    color: #f4f4f7;
    selection-background-color: #f2a93b;
    selection-color: #1a1206;
}

QLineEdit:hover, QComboBox:hover, QSpinBox:hover, QPlainTextEdit:hover, QTextEdit:hover {
    border-color: #3a3a48;
}

QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QPlainTextEdit:focus, QTextEdit:focus {
    border-color: #f2a93b;
}

QLineEdit:disabled, QComboBox:disabled {
    color: #6c6c7c;
    background-color: #15151a;
}

QLineEdit[readOnly="true"] {
    color: #a8a8b8;
    background-color: #141419;
}

QComboBox::drop-down {
    border: none;
    width: 24px;
}

QComboBox QAbstractItemView {
    background-color: #1b1b22;
    border: 1px solid #31313d;
    border-radius: 6px;
    padding: 4px;
    selection-background-color: #2d2d38;
    selection-color: #ffffff;
    outline: none;
}

QListWidget, QListView, QTreeWidget, QTableWidget {
    background-color: transparent;
    border: none;
    outline: none;
}

QListWidget::item, QListView::item {
    border-radius: 8px;
    padding: 6px;
}

QListWidget::item:hover, QListView::item:hover {
    background-color: #22222b;
}

QListWidget::item:selected, QListView::item:selected {
    background-color: #2b2b36;
    color: #ffffff;
}

QTableWidget {
    background-color: #17171e;
    border: 1px solid #2e2e3a;
    border-radius: 8px;
    gridline-color: #262631;
}

QHeaderView::section {
    background-color: #1d1d25;
    color: #a8a8b8;
    border: none;
    border-bottom: 1px solid #2e2e3a;
    padding: 7px;
    font-weight: 600;
}

QScrollArea {
    border: none;
    background: transparent;
}

QScrollArea > QWidget > QWidget {
    background: transparent;
}

QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 2px;
}

QScrollBar::handle:vertical {
    background: #343440;
    border-radius: 4px;
    min-height: 28px;
}

QScrollBar::handle:vertical:hover {
    background: #454555;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    background: transparent;
    height: 0;
}

QScrollBar:horizontal {
    background: transparent;
    height: 10px;
    margin: 2px;
}

QScrollBar::handle:horizontal {
    background: #343440;
    border-radius: 4px;
    min-width: 28px;
}

QTabWidget::pane {
    border: none;
    background: transparent;
}

QTabBar::tab {
    background: transparent;
    color: #9a9aab;
    border: none;
    border-bottom: 2px solid transparent;
    padding: 9px 16px;
    margin-right: 4px;
}

QTabBar::tab:hover {
    color: #f4f4f7;
}

QTabBar::tab:selected {
    color: #f2a93b;
    border-bottom: 2px solid #f2a93b;
    font-weight: 600;
}

QGroupBox {
    border: 1px solid #2e2e3a;
    border-radius: 10px;
    margin-top: 14px;
    padding: 14px 12px 12px 12px;
    background-color: #17171e;
}

QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px;
    padding: 0 4px;
    color: #a8a8b8;
    font-weight: 600;
}

QCheckBox, QRadioButton {
    spacing: 8px;
    color: #e6e6ee;
}

QCheckBox::indicator, QRadioButton::indicator {
    width: 17px;
    height: 17px;
    border: 1px solid #3a3a48;
    border-radius: 5px;
    background-color: #17171e;
}

QCheckBox::indicator:hover, QRadioButton::indicator:hover {
    border-color: #f2a93b;
}

QCheckBox::indicator:checked, QRadioButton::indicator:checked {
    background-color: #f2a93b;
    border-color: #f2a93b;
    image: url(:/assets/check.svg);
}

QRadioButton::indicator {
    border-radius: 9px;
}

QProgressBar {
    background-color: #1b1b22;
    border: none;
    border-radius: 5px;
    height: 8px;
    text-align: center;
    color: transparent;
}

QProgressBar::chunk {
    background-color: #f2a93b;
    border-radius: 5px;
}

QMenu {
    background-color: #1b1b22;
    border: 1px solid #31313d;
    border-radius: 8px;
    padding: 6px;
}

QMenu::item {
    padding: 7px 26px 7px 30px;
    border-radius: 5px;
}

QMenu::item:selected {
    background-color: #2c2c37;
}

QMenu::item:disabled {
    color: #666677;
}

QMenu::separator {
    height: 1px;
    background-color: #2e2e3a;
    margin: 5px 8px;
}

QToolTip {
    background-color: #24242d;
    color: #f4f4f7;
    border: 1px solid #3a3a48;
    border-radius: 6px;
    padding: 5px 8px;
}

QStatusBar {
    background-color: #101014;
    border-top: 1px solid #23232c;
    color: #9a9aab;
}

QStatusBar::item {
    border: none;
}

QSplitter::handle {
    background-color: #23232c;
}

QStackedWidget {
    background: transparent;
}

QFrame#separator {
    background-color: #2a2a34;
    max-height: 1px;
    min-height: 1px;
    border: none;
}

QFrame#settingRow {
    background-color: #17171e;
    border: 1px solid #292934;
    border-radius: 9px;
}

QLabel#settingLabel {
    font-size: 10pt;
}

QLabel#settingHint {
    color: #8e8e9f;
    font-size: 9pt;
}

QDialogButtonBox QPushButton {
    min-width: 92px;
}
)");
}

void apply(QApplication &application)
{
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette palette;
    palette.setColor(QPalette::Window, Background);
    palette.setColor(QPalette::WindowText, Text);
    palette.setColor(QPalette::Base, BackgroundElevated);
    palette.setColor(QPalette::AlternateBase, Surface);
    palette.setColor(QPalette::ToolTipBase, Surface);
    palette.setColor(QPalette::ToolTipText, Text);
    palette.setColor(QPalette::Text, Text);
    palette.setColor(QPalette::Button, Surface);
    palette.setColor(QPalette::ButtonText, Text);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, Accent);
    palette.setColor(QPalette::Highlight, Accent);
    palette.setColor(QPalette::HighlightedText, QColor(0x1a, 0x12, 0x06));
    palette.setColor(QPalette::PlaceholderText, TextMuted);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x66, 0x66, 0x77));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x66, 0x66, 0x77));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x66, 0x66, 0x77));
    application.setPalette(palette);

    application.setStyleSheet(styleSheet());

    QFont font = application.font();
    font.setPointSizeF(10.0);
    application.setFont(font);
}

} // namespace Theme
