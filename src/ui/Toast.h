#pragma once

#include <QLabel>

class QTimer;

class Toast : public QLabel
{
    Q_OBJECT
public:
    explicit Toast(QWidget *parent);

    void showMessage(const QString &message, int durationMs = 3000);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void reposition();

    QTimer *m_hideTimer;
};
