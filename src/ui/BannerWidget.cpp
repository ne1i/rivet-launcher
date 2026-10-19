#include "BannerWidget.h"

#include "Icons.h"
#include "Theme.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

BannerWidget::BannerWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(230);
    setMaximumHeight(280);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void BannerWidget::setImagePath(const QString &path)
{
    m_cover = QPixmap();
    if (!path.isEmpty())
        m_cover.load(path);
    update();
}

void BannerWidget::setTitle(const QString &title)
{
    m_title = title;
    update();
}

void BannerWidget::setSubtitle(const QString &subtitle)
{
    m_subtitle = subtitle;
    update();
}

QSize BannerWidget::sizeHint() const { return QSize(900, 250); }

void BannerWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF area = rect();
    QPainterPath clip;
    clip.addRoundedRect(area, 14, 14);
    painter.setClipPath(clip);

    if (!m_cover.isNull()) {
        const QPixmap scaled = m_cover.scaled(area.size().toSize() * devicePixelRatioF(),
                                              Qt::KeepAspectRatioByExpanding,
                                              Qt::SmoothTransformation);
        const QRectF source((scaled.width() - area.width() * devicePixelRatioF()) / 2.0,
                            (scaled.height() - area.height() * devicePixelRatioF()) / 2.0,
                            area.width() * devicePixelRatioF(),
                            area.height() * devicePixelRatioF());
        painter.drawPixmap(area, scaled, source);
    } else {
        QLinearGradient gradient(area.topLeft(), area.bottomRight());
        gradient.setColorAt(0.0, QColor(0x2a, 0x24, 0x18));
        gradient.setColorAt(1.0, QColor(0x16, 0x16, 0x1c));
        painter.fillRect(area, gradient);
        painter.setOpacity(0.18);
        painter.drawPixmap(QPointF(area.center().x() - 40, area.center().y() - 40),
                           Icons::pixmap(Icons::Name::Gamepad, Qt::white, 80));
        painter.setOpacity(1.0);
    }

    QLinearGradient overlay(area.left(), area.top(), area.left(), area.bottom());
    overlay.setColorAt(0.0, QColor(0x10, 0x10, 0x14, 30));
    overlay.setColorAt(0.45, QColor(0x10, 0x10, 0x14, 80));
    overlay.setColorAt(1.0, QColor(0x10, 0x10, 0x14, 235));
    painter.fillRect(area, overlay);

    painter.setClipping(false);
    if (!m_title.isEmpty()) {
        QFont font = painter.font();
        font.setPointSizeF(24);
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(Theme::Text);
        const QRect textArea = rect().adjusted(28, 0, -28, -54);
        painter.drawText(textArea, Qt::AlignLeft | Qt::AlignBottom,
                         QFontMetrics(font).elidedText(m_title, Qt::ElideRight, textArea.width()));
    }
    if (!m_subtitle.isEmpty()) {
        QFont font = painter.font();
        font.setPointSizeF(9.5);
        font.setBold(false);
        painter.setFont(font);
        painter.setPen(Theme::TextMuted);
        const QRect textArea = rect().adjusted(29, 0, -28, -28);
        painter.drawText(textArea, Qt::AlignLeft | Qt::AlignBottom,
                         QFontMetrics(font).elidedText(m_subtitle, Qt::ElideRight, textArea.width()));
    }
}
