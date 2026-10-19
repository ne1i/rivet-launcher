#pragma once

#include "Config.h"

#include <QPixmap>
#include <QWidget>

class GameCard : public QWidget
{
    Q_OBJECT
public:
    explicit GameCard(const Game &game, QWidget *parent = nullptr);

    void setGame(const Game &game);
    const Game &game() const { return m_game; }
    QSize sizeHint() const override;

signals:
    void clicked(const QString &gameName);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    Game m_game;
    QPixmap m_cover;
    QPixmap m_icon;
    bool m_hovered = false;

    void reloadImages();
    void drawCover(QPainter &painter, const QRectF &rect);
};
