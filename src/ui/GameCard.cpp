#include "GameCard.h"

#include "Icons.h"
#include "Theme.h"
#include "Utils.h"

#include <QFontMetrics>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

namespace {
constexpr int kWidth = 264;
constexpr int kHeight = 198;
constexpr int kFooter = 54;
} // namespace

GameCard::GameCard(const Game &game, QWidget *parent)
    : QWidget(parent)
    , m_game(game)
{
    setFixedSize(kWidth, kHeight);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::StrongFocus);
    setToolTip(QStringLiteral("%1\n%2").arg(game.name, game.executable));
    reloadImages();
}

void GameCard::setGame(const Game &game)
{
    m_game = game;
    setToolTip(QStringLiteral("%1\n%2").arg(game.name, game.executable));
    reloadImages();
    update();
}

QSize GameCard::sizeHint() const { return QSize(kWidth, kHeight); }

void GameCard::reloadImages()
{
    m_cover = QPixmap();
    m_icon = QPixmap();
    if (!m_game.banner.isEmpty() && QFileInfo::exists(m_game.banner))
        m_cover.load(m_game.banner);
    if (!m_game.icon.isEmpty() && QFileInfo::exists(m_game.icon))
        m_icon.load(m_game.icon);
}

void GameCard::drawCover(QPainter &painter, const QRectF &rect)
{
    QPainterPath clip;
    clip.addRoundedRect(rect, 12, 12);
    // Square off the bottom corners so the footer joins seamlessly.
    QPainterPath square;
    square.addRect(QRectF(rect.left(), rect.bottom() - 14, rect.width(), 14));
    clip = clip.united(square);

    painter.save();
    painter.setClipPath(clip);

    if (!m_cover.isNull()) {
        const QSizeF target = rect.size();
        const QPixmap scaled = m_cover.scaled(target.toSize() * devicePixelRatioF(),
                                              Qt::KeepAspectRatioByExpanding,
                                              Qt::SmoothTransformation);
        const QRectF source((scaled.width() - target.width() * devicePixelRatioF()) / 2.0,
                            (scaled.height() - target.height() * devicePixelRatioF()) / 2.0,
                            target.width() * devicePixelRatioF(),
                            target.height() * devicePixelRatioF());
        painter.drawPixmap(rect, scaled, source);
    } else {
        const int hue = qHash(m_game.name) % 360;
        QLinearGradient gradient(rect.topLeft(), rect.bottomRight());
        gradient.setColorAt(0.0, QColor::fromHsv(hue, 80, 52));
        gradient.setColorAt(1.0, QColor::fromHsv((hue + 38) % 360, 90, 28));
        painter.fillRect(rect, gradient);
        if (!m_icon.isNull()) {
            const int side = qMin(96, int(rect.height() * 0.58));
            const QPixmap icon = m_icon.scaled(side * devicePixelRatioF(), side * devicePixelRatioF(),
                                               Qt::KeepAspectRatio, Qt::SmoothTransformation);
            painter.setOpacity(0.9);
            painter.drawPixmap(QPointF(rect.center().x() - side / 2.0,
                                       rect.center().y() - side / 2.0 - 6),
                               icon);
            painter.setOpacity(1.0);
        } else {
            painter.setOpacity(0.22);
            const QPixmap icon = Icons::pixmap(Icons::Name::Gamepad, Qt::white, 46);
            painter.drawPixmap(QPointF(rect.center().x() - 23, rect.center().y() - 23), icon);
            painter.setOpacity(1.0);
        }
    }

    QLinearGradient overlay(rect.left(), rect.top(), rect.left(), rect.bottom());
    overlay.setColorAt(0.55, QColor(0, 0, 0, 0));
    overlay.setColorAt(1.0, QColor(0, 0, 0, 90));
    painter.fillRect(rect, overlay);
    painter.restore();
}

void GameCard::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF outer = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QRectF cover(outer.left(), outer.top(), outer.width(), outer.height() - kFooter);
    const QRectF footer(outer.left(), cover.bottom(), outer.width(), kFooter);

    QPainterPath background;
    background.addRoundedRect(outer, 12, 12);
    painter.fillPath(background, m_hovered ? Theme::SurfaceHover : Theme::Surface);

    drawCover(painter, cover);

    painter.save();
    painter.setClipRect(cover);
    painter.restore();

    // Footer
    painter.setPen(Theme::Text);
    const QPointF iconPoint(footer.left() + 13, footer.top() + 13);
    const QRectF iconRect(iconPoint, QSizeF(28, 28));
    if (!m_icon.isNull()) {
        QPainterPath iconClip;
        iconClip.addRoundedRect(iconRect, 7, 7);
        painter.save();
        painter.setClipPath(iconClip);
        painter.drawPixmap(iconRect.toRect(), m_icon);
        painter.restore();
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0x2c, 0x2c, 0x38));
        painter.drawRoundedRect(iconRect, 7, 7);
        const QPixmap fallback = Icons::pixmap(Icons::Name::Game, Theme::TextMuted, 16);
        painter.drawPixmap(QPointF(iconPoint.x() + 6, iconPoint.y() + 6), fallback);
    }

    QFont titleFont = font();
    titleFont.setPointSizeF(10.5);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(m_hovered ? Theme::AccentHover : Theme::Text);

    const int textLeft = int(iconRect.right()) + 10;
    const int timeWidth = 62;
    const QRect titleRect(textLeft, int(footer.top()) + 10,
                          int(footer.right()) - textLeft - timeWidth - 10, 20);
    const QString title = QFontMetrics(titleFont).elidedText(m_game.name, Qt::ElideRight,
                                                             titleRect.width());
    painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, title);

    QFont smallFont = font();
    smallFont.setPointSizeF(8.5);
    smallFont.setBold(false);
    painter.setFont(smallFont);
    painter.setPen(Theme::TextMuted);
    const QString subtitle = m_game.proton.isEmpty() ? QStringLiteral("Proton") : m_game.proton;
    painter.drawText(QRect(titleRect.left(), titleRect.bottom() + 1, titleRect.width(), 16),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QFontMetrics(smallFont).elidedText(subtitle, Qt::ElideRight, titleRect.width()));

    painter.setFont(smallFont);
    painter.setPen(Theme::TextMuted);
    painter.drawText(QRect(int(footer.right()) - timeWidth - 13, int(footer.top()) + 10,
                           timeWidth, 20),
                     Qt::AlignRight | Qt::AlignVCenter,
                     Utils::humanDuration(m_game.timeSpent));

    if (m_hovered) {
        painter.setPen(QPen(Theme::Accent, 1.4));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(outer, 12, 12);
    }

    if (hasFocus()) {
        painter.setPen(QPen(Theme::Accent, 1.0, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(outer.adjusted(2, 2, -2, -2), 10, 10);
    }
}

void GameCard::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()))
        emit clicked(m_game.name);
    QWidget::mouseReleaseEvent(event);
}

void GameCard::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
        || event->key() == Qt::Key_Space) {
        emit clicked(m_game.name);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void GameCard::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void GameCard::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}
