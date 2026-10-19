#include "LogWindow.h"

#include "Icons.h"
#include "Paths.h"
#include "Theme.h"
#include "Utils.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

LogWindow::LogWindow(const QString &gameName, const QString &initialLog, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_gameName(gameName)
{
    setWindowTitle(QStringLiteral("%1 log — Rivet").arg(gameName));
    resize(920, 620);
    setAttribute(Qt::WA_DeleteOnClose);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(10);

    m_title = new QLabel(QStringLiteral("%1 log").arg(gameName));
    m_title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(m_title);

    m_subtitle = new QLabel(QStringLiteral(
        "Standard output and errors from the game process. Save this file when asking for help."));
    m_subtitle->setObjectName(QStringLiteral("muted"));
    m_subtitle->setWordWrap(true);
    layout->addWidget(m_subtitle);

    m_text = new QPlainTextEdit;
    m_text->setReadOnly(true);
    m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_text->setMaximumBlockCount(50000);
    m_text->setStyleSheet(QStringLiteral(
        "font-family: monospace; font-size: 9.5pt; background: #0d0d11; color: #d6d6e0;"
        " border: 1px solid #2e2e3a; border-radius: 8px; padding: 8px;"));
    m_text->setPlainText(initialLog);
    layout->addWidget(m_text, 1);

    auto *buttons = new QHBoxLayout;
    auto *save = new QPushButton(QStringLiteral("Save log…"));
    save->setIcon(Icons::icon(Icons::Name::Save, Theme::TextMuted, 16));
    connect(save, &QPushButton::clicked, this, &LogWindow::saveLog);
    auto *copy = new QPushButton(QStringLiteral("Copy"));
    copy->setIcon(Icons::icon(Icons::Name::Copy, Theme::TextMuted, 16));
    connect(copy, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(m_text->toPlainText());
    });
    auto *clear = new QPushButton(QStringLiteral("Clear"));
    connect(clear, &QPushButton::clicked, m_text, &QPlainTextEdit::clear);
    buttons->addWidget(save);
    buttons->addWidget(copy);
    buttons->addWidget(clear);
    buttons->addStretch();
    auto *close = new QPushButton(QStringLiteral("Close"));
    close->setObjectName(QStringLiteral("primary"));
    connect(close, &QPushButton::clicked, this, &QWidget::close);
    buttons->addWidget(close);
    layout->addLayout(buttons);

    scrollToEnd();
}

void LogWindow::setGameName(const QString &name)
{
    m_gameName = name;
    setWindowTitle(QStringLiteral("%1 log — Rivet").arg(name));
    m_title->setText(QStringLiteral("%1 log").arg(name));
}

void LogWindow::setLog(const QString &text)
{
    m_text->setPlainText(text);
    scrollToEnd();
}

void LogWindow::appendLine(const QString &line, bool stderrOutput)
{
    QScrollBar *bar = m_text->verticalScrollBar();
    const bool atBottom = bar->value() >= bar->maximum() - 4;
    m_text->appendPlainText((stderrOutput ? QStringLiteral("[err] ") : QStringLiteral("[out] "))
                            + line);
    if (atBottom)
        scrollToEnd();
}

void LogWindow::scrollToEnd()
{
    m_text->verticalScrollBar()->setValue(m_text->verticalScrollBar()->maximum());
}

void LogWindow::saveLog()
{
    const QString directory = Paths::documentsDir() + QStringLiteral("/Rivet Logs");
    if (!QDir().mkpath(directory)) {
        QMessageBox::critical(this, QStringLiteral("Save log"),
                              QStringLiteral("Could not create %1").arg(directory));
        return;
    }

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH-mm"));
    const QString path = directory + QLatin1Char('/') + m_gameName + QLatin1Char(' ')
        + stamp + QStringLiteral(".log");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, QStringLiteral("Save log"), file.errorString());
        return;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << m_text->toPlainText();
    file.close();

    QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
}
