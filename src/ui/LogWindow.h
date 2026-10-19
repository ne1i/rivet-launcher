#pragma once

#include <QWidget>

class QLabel;
class QPlainTextEdit;

class LogWindow : public QWidget
{
    Q_OBJECT
public:
    explicit LogWindow(const QString &gameName, const QString &initialLog = QString(),
                       QWidget *parent = nullptr);

    void setGameName(const QString &name);
    void setLog(const QString &text);
    void appendLine(const QString &line, bool stderrOutput = false);
    void scrollToEnd();

private:
    void saveLog();

    QString m_gameName;
    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QPlainTextEdit *m_text = nullptr;
};
