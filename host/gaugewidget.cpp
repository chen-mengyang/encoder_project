#include "gaugewidget.h"

#include <QPainter>
#include <QtMath>
#include <QFontMetricsF>

AngleGauge::AngleGauge(QWidget *parent)
    : QWidget(parent)
{
}

void AngleGauge::setAngle(double degrees)
{
    if (degrees < 0)
        degrees = 0;
    if (degrees > 360)
        degrees = 360;
    if (qFuzzyCompare(m_angle + 1.0, degrees + 1.0))
        return;
    m_angle = degrees;
    update();
}

QSize AngleGauge::minimumSizeHint() const
{
    return QSize(220, 220);
}

void AngleGauge::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int side = qMin(width(), height());
    const qreal r = (side - 2 * 18) / 2.0;
    const QPointF c(width() / 2.0, height() / 2.0);

    // 表盘背景
    p.setBrush(QColor(245, 247, 250));
    p.setPen(QPen(QColor(170, 178, 190), 2));
    p.drawEllipse(c, r, r);

    // 刻度与数字（0 在上方，顺时针）
    QFont f = p.font();
    f.setPointSizeF(9);
    p.setFont(f);
    for (int deg = 0; deg < 360; deg += 5) {
        const qreal rad = qDegreesToRadians(qreal(deg));
        const bool major = (deg % 30 == 0);
        const qreal r1 = r - (major ? 10 : 4);
        const qreal r2 = r - 14;
        const QPointF p1(c.x() + r1 * qSin(rad), c.y() - r1 * qCos(rad));
        const QPointF p2(c.x() + r2 * qSin(rad), c.y() - r2 * qCos(rad));
        p.setPen(QPen(major ? QColor(90, 98, 110) : QColor(150, 158, 170),
                      major ? 2.0 : 1.0));
        p.drawLine(p1, p2);
        if (major) {
            const qreal rt = r - 26;
            const QPointF tp(c.x() + rt * qSin(rad), c.y() - rt * qCos(rad));
            const QString label = QString::number(deg);
            const QFontMetricsF fm(p.font());
            const QRectF br = fm.boundingRect(label);
            p.drawText(QRectF(tp.x() - br.width() / 2.0,
                              tp.y() - br.height() / 2.0,
                              br.width(), br.height()),
                       Qt::AlignCenter, label);
        }
    }

    // 指针
    const qreal rad = qDegreesToRadians(m_angle);
    const QPointF dir(qSin(rad), -qCos(rad));   // 指针方向
    const QPointF perp(qCos(rad), qSin(rad));   // 垂直方向
    const QPointF tip = c + dir * (r - 22);
    const QPointF base1 = c + perp * 9;
    const QPointF base2 = c - perp * 9;
    QPolygonF tri;
    tri << tip << base1 << base2;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(231, 76, 60));
    p.drawPolygon(tri);

    // 中心
    p.setBrush(QColor(52, 73, 94));
    p.drawEllipse(c, 6, 6);
    p.setBrush(QColor(245, 247, 250));
    p.drawEllipse(c, 3, 3);
}
