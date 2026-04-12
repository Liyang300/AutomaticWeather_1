#include "CustomChartWidget.h"
#include <QVBoxLayout>
#include <QDateTime>
#include <QTimeZone>
#include <limits>
#include <QDebug>
#include <utility>   // for std::as_const

CustomChartWidget::CustomChartWidget(QWidget *parent)
    : QWidget(parent)
    , m_chart(nullptr)
    , m_chartView(nullptr)
    , m_minSeries(nullptr)
    , m_maxSeries(nullptr)
    , m_axisX(nullptr)
    , m_axisY(nullptr)
{
    initializeChart();

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_chartView);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);
}

CustomChartWidget::~CustomChartWidget()
{
}

void CustomChartWidget::initializeChart()
{
    m_chart = new QChart();
    m_chart->setTheme(QChart::ChartThemeLight);
    m_chart->setAnimationOptions(QChart::SeriesAnimations);

    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    m_minSeries = new QLineSeries();
    m_minSeries->setName("最小值");
    m_minSeries->setColor(Qt::blue);
    m_minSeries->setPen(QPen(Qt::blue, 2));
    m_minSeries->setPointsVisible(true);

    m_maxSeries = new QLineSeries();
    m_maxSeries->setName("最大值");
    m_maxSeries->setColor(Qt::red);
    m_maxSeries->setPen(QPen(Qt::red, 2));
    m_maxSeries->setPointsVisible(true);

    m_axisX = new QDateTimeAxis();
    m_axisX->setFormat("MM-dd");
    m_axisX->setTitleText("日期");

    m_axisY = new QValueAxis();
    m_axisY->setTitleText("数值");

    m_chart->addSeries(m_minSeries);
    m_chart->addSeries(m_maxSeries);
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    m_minSeries->attachAxis(m_axisX);
    m_minSeries->attachAxis(m_axisY);
    m_maxSeries->attachAxis(m_axisX);
    m_maxSeries->attachAxis(m_axisY);

    m_chart->setMargins(QMargins(0, 0, 0, 0));
    m_chartView->setContentsMargins(0, 0, 0, 0);
}

void CustomChartWidget::clearChart()
{
    m_minSeries->clear();
    m_maxSeries->clear();
}

void CustomChartWidget::updateChart(const QVector<QVector<QPointF>> &data, int dataType, int maxDays)
{
    qDebug() << "updateChart: 数据天数 =" << data.size();
    clearChart();

    // 设置 Y 轴标题
    QString yTitle;
    switch (dataType) {
    case 0: yTitle = "温度 (℃)"; break;
    case 1: yTitle = "湿度 (%RH)"; break;
    case 2: yTitle = "大气压 (kPa)"; break;
    case 3: yTitle = "含水量 (g/m³)"; break;
    case 4: yTitle = "含水量百分比 (%)"; break;
    case 5: yTitle = "露点温度 (℃)"; break;
    default: yTitle = "数值"; break;
    }
    m_axisY->setTitleText(yTitle);

    if (data.isEmpty()) {
        m_chart->setTitle("无数据");
        return;
    }

    // --- 计算固定日期范围（最近 maxDays 天）---
    QDate today = QDate::currentDate();
    QDate startDate = today.addDays(-(maxDays - 1)); // 例如 maxDays=7，则 startDate = today-6

    QTimeZone localZone = QTimeZone::systemTimeZone(); // 获取系统时区
    QDateTime start = startDate.startOfDay(localZone);
    QDateTime end   = today.startOfDay(localZone);

    // 设置 X 轴范围
    m_axisX->setRange(start, end);
    m_axisX->setTickCount(maxDays); // 固定显示 maxDays 个刻度

    // --- 按日期升序排序传入的数据 ---
    QVector<QVector<QPointF>> sortedData = data;
    std::sort(sortedData.begin(), sortedData.end(),
              [](const QVector<QPointF>& a, const QVector<QPointF>& b) {
                  return a[0].x() < b[0].x();
              });

    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    qreal startMsec = start.toMSecsSinceEpoch();
    qreal endMsec   = end.toMSecsSinceEpoch();

    // --- 添加数据点，仅保留在固定范围内的点 ---
    for (const auto &day : std::as_const(sortedData)) {
        if (day.size() >= 2) {
            qreal timestamp = day[0].x(); // 该日期0点时间戳
            if (timestamp >= startMsec && timestamp <= endMsec) {
                m_minSeries->append(timestamp, day[0].y());
                m_maxSeries->append(timestamp, day[1].y());

                minY = qMin(minY, qMin(day[0].y(), day[1].y()));
                maxY = qMax(maxY, qMax(day[0].y(), day[1].y()));
            }
        }
    }

    // 如果范围内没有任何点，直接返回
    if (minY == std::numeric_limits<double>::max() || maxY == std::numeric_limits<double>::lowest()) {
        m_chart->setTitle("范围内无数据");
        return;
    }

    // --- 设置 Y 轴范围 ---
    if (dataType == 1 || dataType == 4) { // 湿度或含水量百分比固定 0-100
        m_axisY->setRange(0, 100);
    } else {
        double margin = (maxY - minY) * 0.1;
        if (margin == 0) margin = 1.0;
        m_axisY->setRange(minY - margin, maxY + margin);
    }

    // --- 设置图表标题 ---
    QString typeName;
    switch (dataType) {
    case 0: typeName = "温度"; break;
    case 1: typeName = "湿度"; break;
    case 2: typeName = "大气压"; break;
    case 3: typeName = "含水量"; break;
    case 4: typeName = "含水量百分比"; break;
    case 5: typeName = "露点温度"; break;
    default: typeName = "数据"; break;
    }
    QString title = QString("%1 - 最近%2天极值变化")
                        .arg(typeName)
                        .arg(maxDays);
    setChartTitle(title);

    qDebug() << "min系列点数 =" << m_minSeries->count() << "max系列点数 =" << m_maxSeries->count();
}

void CustomChartWidget::setChartTitle(const QString &title)
{
    m_chart->setTitle(title);
}
