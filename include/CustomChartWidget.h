#ifndef CUSTOMCHARTWIDGET_H
#define CUSTOMCHARTWIDGET_H

#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QVector>
#include <QPointF>

class CustomChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CustomChartWidget(QWidget *parent = nullptr);
    ~CustomChartWidget();

    void updateChart(const QVector<QVector<QPointF>> &data, int dataType, int maxDays);
    void setChartTitle(const QString &title);

private:
    QChart         *m_chart;
    QChartView     *m_chartView;
    QLineSeries    *m_minSeries;
    QLineSeries    *m_maxSeries;
    QDateTimeAxis  *m_axisX;       // 改回 QDateTimeAxis
    QValueAxis     *m_axisY;

    void initializeChart();
    void clearChart();
};

#endif // CUSTOMCHARTWIDGET_H
