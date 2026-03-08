// CompassWidget.cpp
#include "CompassWidget.h"
#include <QDebug>

CompassWidget::CompassWidget(QWidget *parent)
    : QWidget(parent)
    , m_windSpeed(0)
    , m_windDirection(0)
{
    setMinimumSize(100, 100);
    setMaximumSize(250, 250);
}

void CompassWidget::setWindSpeed(float speed)
{
    m_windSpeed = speed;
    update(); // 触发重绘
}

void CompassWidget::setWindDirection(int direction)
{
    m_windDirection = direction;
    update();
}

void CompassWidget::setWindDirectionStr(const QString& directionStr)
{
    m_windDirectionStr = directionStr;
}

void CompassWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    //向上偏移20像素
    int verticalOffset = 10;  // 向上偏移20像素
    painter.translate(0, -verticalOffset);

    // 绘制罗盘背景
    drawCompass(painter);

    // 绘制方向标签
    drawDirectionLabels(painter);

    // 只有在风速大于0时才显示风指针
    if (m_windSpeed > 0.1) {
        drawWindArrow(painter);
    }

    // 在中心显示风向文字
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 10, QFont::Bold));

    QString displayText;
    if (m_windSpeed <= 0.1) {
        displayText = "无风";
    } else {
        displayText = m_windDirectionStr.isEmpty() ?
                          QString::number(m_windDirection) + "°" :
                          m_windDirectionStr;
    }

    // 中心矩形位置
    QRect centerRect(width()/2 - 40, height()/2 - 15, 80, 30);
    painter.drawText(centerRect, Qt::AlignCenter, displayText);

    // 显示风速 - 调整位置，确保在罗盘下方且不会被覆盖
    painter.setFont(QFont("Arial", 10, QFont::Normal));  // 使用正常字体，不是粗体

    QString speedText;
    if (m_windSpeed <= 0.1) {
        speedText = "风速: 0.0 m/s";
    } else {
        speedText = QString("风速: %1 m/s").arg(m_windSpeed, 0, 'f', 1);
    }

    // 计算罗盘底部位置
    qreal centerX = width() / 2.0;
    qreal centerY = height() / 2.0;
    qreal radius = qMin(centerX, centerY) * 0.8;

    // 罗盘底部Y坐标
    qreal compassBottomY = centerY + radius;

    // 风速文本显示在罗盘下方，但要考虑字体高度
    QFontMetrics fm(painter.font());
    int textHeight = fm.height();

    // 风速文本矩形 - 在罗盘下方适当距离
    QRect speedRect(centerX - 60,
                    compassBottomY + 5,  // 距离罗盘底部5像素
                    120,
                    textHeight + 5);

    // 绘制半透明背景，使文字更清晰
    painter.setBrush(QColor(255, 255, 255, 200));  // 半透明白色
    painter.setPen(Qt::NoPen);
    painter.drawRect(speedRect);

    // 绘制风速文本
    painter.setPen(Qt::blue);
    painter.drawText(speedRect, Qt::AlignCenter, speedText);
}

void CompassWidget::drawCompass(QPainter &painter)
{
    qreal centerX = width() / 2.0;
    qreal centerY = height() / 2.0;
    qreal radius = qMin(centerX, centerY) * 0.6;   //罗盘半径

    // 绘制外圆
    painter.setPen(QPen(Qt::darkGray, 2));
    painter.setBrush(QColor(240, 240, 240));
    painter.drawEllipse(QPointF(centerX, centerY), radius, radius);

    // 绘制刻度线
    painter.setPen(QPen(Qt::black, 1));
    for (int i = 0; i < 360; i += 30) {
        qreal radian = qDegreesToRadians(static_cast<qreal>(i));
        qreal innerRadius = radius - 15;
        qreal outerRadius = radius;

        qreal x1 = centerX + innerRadius * qSin(radian);
        qreal y1 = centerY - innerRadius * qCos(radian);
        qreal x2 = centerX + outerRadius * qSin(radian);
        qreal y2 = centerY - outerRadius * qCos(radian);

        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }
}

void CompassWidget::drawDirectionLabels(QPainter &painter)
{
    qreal centerX = width() / 2.0;
    qreal centerY = height() / 2.0;
    qreal radius = qMin(centerX, centerY) * 0.8;

    // 让字母在罗盘外圈
    // textRadius = qMin(radius + 15, maxRadius);
    qreal availableWidth = width() - 20;
    qreal availableHeight = height() - 40;

    qreal maxRadius = qMin(availableWidth / 2.0, availableHeight / 2.0);
    qreal textRadius = radius + 40;  // 增加这个值让字母离罗盘更远

    // 确保不会超出控件边界
    textRadius = qMin(textRadius, maxRadius + 3);

    // 根据控件大小动态调整字体大小
    int fontSize = qMax(12, qMin(18, static_cast<int>(radius / 4)));
    QFont labelFont("Arial", fontSize, QFont::Bold);
    painter.setFont(labelFont);
    painter.setPen(QPen(Qt::darkBlue, 2));

    QString directions[] = {"N", "E", "S", "W"};
    qreal angles[] = {0, 90, 180, 270};

    for (int i = 0; i < 4; i++) {
        qreal radian = qDegreesToRadians(angles[i]);
        qreal x = centerX + textRadius * qSin(radian);
        qreal y = centerY - textRadius * qCos(radian);

        // 根据字体大小调整文本矩形
        // 注意：文本对齐点是矩形的中心，所以需要偏移
        QFontMetrics fm(labelFont);
        int textWidth = fm.horizontalAdvance(directions[i]);
        int textHeight = fm.height();

        QRectF textRect(x - textWidth/2, y - textHeight/2, textWidth, textHeight);
        painter.drawText(textRect, Qt::AlignCenter, directions[i]);
    }
}


void CompassWidget::drawWindArrow(QPainter &painter)
{
    qreal centerX = width() / 2.0;
    qreal centerY = height() / 2.0;
    qreal radius = qMin(centerX, centerY) * 0.8;

    painter.save();
    painter.translate(centerX, centerY);
    painter.rotate(m_windDirection);

    // 绘制风向箭头
    QPainterPath path;
    path.moveTo(0, -radius + 20);
    path.lineTo(-10, 0);
    path.lineTo(0, 10);
    path.lineTo(10, 0);
    path.closeSubpath();

    // 根据风速设置颜色（风速越大颜色越红）
    int red = qMin(255, static_cast<int>(m_windSpeed * 50));
    QColor arrowColor = QColor(red, 50, 50);

    painter.setBrush(arrowColor);
    painter.setPen(QPen(Qt::darkRed, 2));
    painter.drawPath(path);

    painter.restore();
}
