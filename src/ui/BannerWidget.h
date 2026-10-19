#pragma once

#include <QPixmap>
#include <QWidget>

class BannerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BannerWidget(QWidget *parent = nullptr);

    void setImagePath(const QString &path);
    void setTitle(const QString &title);
    void setSubtitle(const QString &subtitle);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_cover;
    QString m_title;
    QString m_subtitle;
};
