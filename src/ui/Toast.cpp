#include "Toast.h"

#include <QEvent>
#include <QTimer>

Toast::Toast(QWidget *parent)
    : QLabel(parent)
    , m_hideTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("toast"));
    setStyleSheet(QStringLiteral(
        "QLabel#toast { background-color: #2a2a34; color: #f4f4f7; border: 1px solid #3a3a48;"
        " border-radius: 10px; padding: 10px 18px; }"));
    setAlignment(Qt::AlignCenter);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    hide();

    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &QWidget::hide);
    if (parent)
        parent->installEventFilter(this);
}

void Toast::showMessage(const QString &message, int durationMs)
{
    setText(message);
    adjustSize();
    setFixedWidth(qMin(sizeHint().width() + 36, parentWidget()->width() - 40));
    setFixedHeight(qMax(sizeHint().height(), 40));
    reposition();
    raise();
    show();
    m_hideTimer->start(durationMs);
}

void Toast::reposition()
{
    if (!parentWidget())
        return;
    const int x = (parentWidget()->width() - width()) / 2;
    const int y = parentWidget()->height() - height() - 42;
    move(qMax(20, x), qMax(20, y));
}

bool Toast::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize)
        reposition();
    return QLabel::eventFilter(watched, event);
}
