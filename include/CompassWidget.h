// CompassWidget.h
#ifndef COMPASSWIDGET_H
#define COMPASSWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

class CompassWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(float windSpeed READ windSpeed WRITE setWindSpeed)
    Q_PROPERTY(int windDirection READ windDirection WRITE setWindDirection)
    Q_PROPERTY(QString windDirectionStr READ windDirectionStr WRITE setWindDirectionStr)

public:
    explicit CompassWidget(QWidget *parent = nullptr);

    float windSpeed() const { return m_windSpeed; }
    int windDirection() const { return m_windDirection; }
    QString windDirectionStr() const { return m_windDirectionStr; }

    void setWindSpeed(float speed);
    void setWindDirection(int direction);
    void setWindDirectionStr(const QString& directionStr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    float m_windSpeed;
    int m_windDirection;
    QString m_windDirectionStr;

    void drawCompass(QPainter &painter);
    void drawWindArrow(QPainter &painter);
    void drawDirectionLabels(QPainter &painter);
};

#endif // COMPASSWIDGET_H
