#include "FlowLayout.h"

#include <QStyle>
#include <QWidget>

FlowLayout::FlowLayout(QWidget *parent, int margin, int hSpacing, int vSpacing)
    : QLayout(parent)
    , m_hSpacing(hSpacing)
    , m_vSpacing(vSpacing)
{
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout()
{
    while (QLayoutItem *item = takeAt(0))
        delete item;
}

void FlowLayout::addItem(QLayoutItem *item) { m_items.append(item); }

int FlowLayout::count() const { return m_items.size(); }

QLayoutItem *FlowLayout::itemAt(int index) const
{
    return index >= 0 && index < m_items.size() ? m_items.at(index) : nullptr;
}

QLayoutItem *FlowLayout::takeAt(int index)
{
    if (index < 0 || index >= m_items.size())
        return nullptr;
    return m_items.takeAt(index);
}

Qt::Orientations FlowLayout::expandingDirections() const { return {}; }

bool FlowLayout::hasHeightForWidth() const { return true; }

int FlowLayout::heightForWidth(int width) const
{
    return doLayout(QRect(0, 0, width, 0), true);
}

void FlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

QSize FlowLayout::sizeHint() const { return minimumSize(); }

QSize FlowLayout::minimumSize() const
{
    QSize size;
    for (const QLayoutItem *item : m_items) {
        if (item->isEmpty())
            continue;
        size = size.expandedTo(item->minimumSize());
    }
    const QMargins margins = contentsMargins();
    size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
    return size;
}

int FlowLayout::horizontalSpacing() const
{
    if (m_hSpacing >= 0)
        return m_hSpacing;
    return 6;
}

int FlowLayout::verticalSpacing() const
{
    if (m_vSpacing >= 0)
        return m_vSpacing;
    return 6;
}

int FlowLayout::doLayout(const QRect &rect, bool testOnly) const
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    getContentsMargins(&left, &top, &right, &bottom);
    const QRect effective = rect.adjusted(left, top, -right, -bottom);

    int x = effective.x();
    int y = effective.y();
    int lineHeight = 0;
    const int hSpacing = horizontalSpacing();
    const int vSpacing = verticalSpacing();

    for (QLayoutItem *item : m_items) {
        if (item->isEmpty())
            continue;
        const QSize hint = item->sizeHint();
        const int nextX = x + hint.width() + hSpacing;
        if (nextX - hSpacing > effective.right() + 1 && lineHeight > 0) {
            x = effective.x();
            y += lineHeight + vSpacing;
            lineHeight = 0;
        }
        if (!testOnly)
            item->setGeometry(QRect(QPoint(x, y), hint));
        x += hint.width() + hSpacing;
        lineHeight = qMax(lineHeight, hint.height());
    }
    return y + lineHeight - rect.y() + bottom;
}
