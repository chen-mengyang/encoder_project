#ifndef GAUGEWIDGET_H
#define GAUGEWIDGET_H

#include <QWidget>

// 编码器角度表盘控件（0° 在正上方，顺时针增长）
class AngleGauge : public QWidget
{
    Q_OBJECT

public:
    explicit AngleGauge(QWidget *parent = nullptr);

    void setAngle(double degrees);
    double angle() const { return m_angle; }

    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double m_angle = 0.0;
};

#endif // GAUGEWIDGET_H
