#include "Icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>

#include <cmath>

namespace {

void draw(QPainter &p, Icons::Name name)
{
    const QColor iconColor = p.pen().color();
    QPen pen(p.pen());
    pen.setWidthF(1.8);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (name) {
    case Icons::Name::Play: {
        QPainterPath path;
        path.moveTo(8, 5.5);
        path.lineTo(19, 12);
        path.lineTo(8, 18.5);
        path.closeSubpath();
        p.setBrush(p.pen().color());
        p.drawPath(path);
        break;
    }
    case Icons::Name::Stop: {
        p.setBrush(p.pen().color());
        p.drawRoundedRect(QRectF(7, 7, 10, 10), 2, 2);
        break;
    }
    case Icons::Name::Plus:
        p.drawLine(QPointF(12, 5), QPointF(12, 19));
        p.drawLine(QPointF(5, 12), QPointF(19, 12));
        break;
    case Icons::Name::Settings: {
        constexpr int teeth = 8;
        constexpr double outer = 9.4;
        constexpr double root = 6.8;
        const double sector = 2.0 * M_PI / teeth;
        QPainterPath path;
        for (int i = 0; i < teeth; ++i) {
            const double base = i * sector;
            const QPointF p1(12 + root * std::cos(base - sector * 0.44),
                             12 + root * std::sin(base - sector * 0.44));
            const QPointF p2(12 + outer * std::cos(base - sector * 0.21),
                             12 + outer * std::sin(base - sector * 0.21));
            const QPointF p3(12 + outer * std::cos(base + sector * 0.21),
                             12 + outer * std::sin(base + sector * 0.21));
            const QPointF p4(12 + root * std::cos(base + sector * 0.44),
                             12 + root * std::sin(base + sector * 0.44));
            if (i == 0)
                path.moveTo(p1);
            else
                path.lineTo(p1);
            path.lineTo(p2);
            path.lineTo(p3);
            path.lineTo(p4);
        }
        path.closeSubpath();

        QPainterPath hole;
        hole.addEllipse(QPointF(12, 12), 3.0, 3.0);
        path = path.subtracted(hole);

        p.setPen(Qt::NoPen);
        p.setBrush(iconColor);
        p.drawPath(path);
        break;
    }
    case Icons::Name::Trash:
        p.drawLine(QPointF(4.5, 7), QPointF(19.5, 7));
        p.drawLine(QPointF(9, 7), QPointF(9.5, 4.5));
        p.drawLine(QPointF(9.5, 4.5), QPointF(14.5, 4.5));
        p.drawLine(QPointF(14.5, 4.5), QPointF(15, 7));
        p.drawRoundedRect(QRectF(6.5, 7, 11, 12), 2, 2);
        p.drawLine(QPointF(10, 10.5), QPointF(10, 15.5));
        p.drawLine(QPointF(14, 10.5), QPointF(14, 15.5));
        break;
    case Icons::Name::Folder: {
        QPainterPath path;
        path.moveTo(3.5, 7);
        path.lineTo(9, 7);
        path.lineTo(11, 9.5);
        path.lineTo(20.5, 9.5);
        path.lineTo(20.5, 18.5);
        path.lineTo(3.5, 18.5);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case Icons::Name::Debug:
        p.drawRoundedRect(QRectF(5, 6, 14, 12), 4, 4);
        p.drawLine(QPointF(9, 3), QPointF(10, 6));
        p.drawLine(QPointF(15, 3), QPointF(14, 6));
        p.drawLine(QPointF(8, 6), QPointF(7, 2.8));
        p.drawLine(QPointF(16, 6), QPointF(17, 2.8));
        p.drawLine(QPointF(2.5, 10), QPointF(5, 11));
        p.drawLine(QPointF(21.5, 10), QPointF(19, 11));
        p.drawLine(QPointF(2.5, 16), QPointF(5, 15));
        p.drawLine(QPointF(21.5, 16), QPointF(19, 15));
        p.drawLine(QPointF(5, 12), QPointF(5, 15.5));
        p.drawLine(QPointF(19, 12), QPointF(19, 15.5));
        break;
    case Icons::Name::Search:
        p.drawEllipse(QPointF(10.5, 10.5), 6, 6);
        p.drawLine(QPointF(15, 15), QPointF(20, 20));
        break;
    case Icons::Name::Back:
        p.drawLine(QPointF(19, 12), QPointF(5, 12));
        p.drawLine(QPointF(5, 12), QPointF(11, 6));
        p.drawLine(QPointF(5, 12), QPointF(11, 18));
        break;
    case Icons::Name::Download:
        p.drawLine(QPointF(12, 3.5), QPointF(12, 15));
        p.drawLine(QPointF(12, 15), QPointF(7.5, 10.5));
        p.drawLine(QPointF(12, 15), QPointF(16.5, 10.5));
        p.drawLine(QPointF(4, 16.5), QPointF(4, 20));
        p.drawLine(QPointF(4, 20), QPointF(20, 20));
        p.drawLine(QPointF(20, 20), QPointF(20, 16.5));
        break;
    case Icons::Name::Save:
        p.drawRoundedRect(QRectF(4, 4, 16, 16), 2.5, 2.5);
        p.drawRect(QRectF(8, 4, 8, 5));
        p.drawRoundedRect(QRectF(7.5, 13, 9, 7), 1.5, 1.5);
        break;
    case Icons::Name::Edit:
        p.drawLine(QPointF(4, 20), QPointF(7, 19.5));
        p.drawLine(QPointF(7, 19.5), QPointF(20, 6.5));
        p.drawLine(QPointF(20, 6.5), QPointF(17.5, 4));
        p.drawLine(QPointF(17.5, 4), QPointF(4.5, 17));
        p.drawLine(QPointF(4.5, 17), QPointF(4, 20));
        break;
    case Icons::Name::Wine: {
        QPainterPath path;
        path.moveTo(7, 3.5);
        path.lineTo(17, 3.5);
        path.lineTo(16, 11);
        path.cubicTo(15.7, 15, 14, 17.5, 12, 17.5);
        path.cubicTo(10, 17.5, 8.3, 15, 8, 11);
        path.closeSubpath();
        p.drawPath(path);
        p.drawLine(QPointF(12, 17.5), QPointF(12, 20.5));
        p.drawLine(QPointF(8.5, 20.5), QPointF(15.5, 20.5));
        p.drawLine(QPointF(9, 8.5), QPointF(15, 8.5));
        break;
    }
    case Icons::Name::Link:
        p.drawRoundedRect(QRectF(3.5, 9, 8, 6), 3, 3);
        p.drawRoundedRect(QRectF(12.5, 9, 8, 6), 3, 3);
        p.drawLine(QPointF(9, 12), QPointF(15, 12));
        break;
    case Icons::Name::External:
        p.drawLine(QPointF(13, 4), QPointF(20, 4));
        p.drawLine(QPointF(20, 4), QPointF(20, 11));
        p.drawLine(QPointF(20, 4), QPointF(11, 13));
        p.drawLine(QPointF(18, 14), QPointF(18, 19));
        p.drawLine(QPointF(18, 19), QPointF(5, 19));
        p.drawLine(QPointF(5, 19), QPointF(5, 6));
        p.drawLine(QPointF(5, 6), QPointF(10, 6));
        break;
    case Icons::Name::Clock:
        p.drawEllipse(QPointF(12, 12), 8.5, 8.5);
        p.drawLine(QPointF(12, 12), QPointF(12, 7));
        p.drawLine(QPointF(12, 12), QPointF(15.5, 14));
        break;
    case Icons::Name::Terminal:
        p.drawRoundedRect(QRectF(3, 4.5, 18, 15), 2.5, 2.5);
        p.drawLine(QPointF(6.5, 9), QPointF(10, 12));
        p.drawLine(QPointF(10, 12), QPointF(6.5, 15));
        p.drawLine(QPointF(12.5, 15), QPointF(17, 15));
        break;
    case Icons::Name::Run:
        p.drawRoundedRect(QRectF(3, 4, 18, 16), 2.5, 2.5);
        p.drawLine(QPointF(3, 8), QPointF(21, 8));
        p.drawLine(QPointF(7, 12), QPointF(10.5, 14.5));
        p.drawLine(QPointF(10.5, 14.5), QPointF(7, 17));
        break;
    case Icons::Name::Gamepad:
        p.drawRoundedRect(QRectF(2.5, 7, 19, 11), 5, 5);
        p.drawLine(QPointF(7.5, 10), QPointF(7.5, 14));
        p.drawLine(QPointF(5.5, 12), QPointF(9.5, 12));
        p.drawEllipse(QPointF(16.5, 11), 1.1, 1.1);
        p.drawEllipse(QPointF(18.5, 13.5), 1.1, 1.1);
        break;
    case Icons::Name::Close:
        p.drawLine(QPointF(6, 6), QPointF(18, 18));
        p.drawLine(QPointF(18, 6), QPointF(6, 18));
        break;
    case Icons::Name::Refresh:
        p.drawArc(QRectF(4, 4, 16, 16), 40 * 16, 280 * 16);
        p.drawLine(QPointF(18.5, 5), QPointF(18.5, 10));
        p.drawLine(QPointF(18.5, 10), QPointF(13.5, 10));
        break;
    case Icons::Name::Monitor:
        p.drawRoundedRect(QRectF(2.5, 4, 19, 13), 2, 2);
        p.drawLine(QPointF(9, 20.5), QPointF(15, 20.5));
        p.drawLine(QPointF(12, 17), QPointF(12, 20.5));
        break;
    case Icons::Name::Check:
        p.drawLine(QPointF(4.5, 12.5), QPointF(9.5, 17.5));
        p.drawLine(QPointF(9.5, 17.5), QPointF(19.5, 6.5));
        break;
    case Icons::Name::Warning:
        p.drawPolygon(QPolygonF({QPointF(12, 4), QPointF(21, 19.5), QPointF(3, 19.5)}));
        p.drawLine(QPointF(12, 9), QPointF(12, 14));
        p.drawPoint(QPointF(12, 17));
        break;
    case Icons::Name::Info:
        p.drawEllipse(QPointF(12, 12), 8.5, 8.5);
        p.drawLine(QPointF(12, 10.5), QPointF(12, 17));
        p.drawPoint(QPointF(12, 7.5));
        break;
    case Icons::Name::More:
        p.setBrush(p.pen().color());
        p.drawEllipse(QPointF(5.5, 12), 1.4, 1.4);
        p.drawEllipse(QPointF(12, 12), 1.4, 1.4);
        p.drawEllipse(QPointF(18.5, 12), 1.4, 1.4);
        break;
    case Icons::Name::Copy:
        p.drawRoundedRect(QRectF(8, 8, 12, 12), 2, 2);
        p.drawLine(QPointF(16, 8), QPointF(16, 5.5));
        p.drawLine(QPointF(16, 5.5), QPointF(4, 5.5));
        p.drawLine(QPointF(4, 5.5), QPointF(4, 16));
        p.drawLine(QPointF(4, 16), QPointF(8, 16));
        break;
    case Icons::Name::Game:
        p.drawRoundedRect(QRectF(3, 6, 18, 13), 3, 3);
        p.drawLine(QPointF(3, 10), QPointF(21, 10));
        p.drawEllipse(QPointF(8, 15), 1, 1);
        p.drawEllipse(QPointF(12, 15), 1, 1);
        p.drawEllipse(QPointF(16, 15), 1, 1);
        break;
    }
}

} // namespace

namespace Icons {

QPixmap pixmap(Name name, const QColor &color, int size)
{
    constexpr int kScale = 2;
    QPixmap result(size * kScale, size * kScale);
    result.setDevicePixelRatio(kScale);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(double(size) / 24.0, double(size) / 24.0);
    painter.setPen(QPen(color, 1.8));
    painter.setBrush(Qt::NoBrush);
    draw(painter, name);
    painter.end();
    return result;
}

QIcon icon(Name name, const QColor &color, int size)
{
    return QIcon(pixmap(name, color, size));
}

QIcon appIcon()
{
    return QIcon(QStringLiteral(":/assets/rivet.svg"));
}

} // namespace Icons
