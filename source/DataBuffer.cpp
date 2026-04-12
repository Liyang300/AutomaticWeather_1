// DataBuffer.cpp
#include "DataBuffer.h"
#include <QDebug>
#include <QDateTime>

DataBuffer::DataBuffer(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_isRunning(false)
{
    m_timer->setInterval(1000); // 每秒处理一次
    connect(m_timer, &QTimer::timeout, this, &DataBuffer::processBuffer);
}

DataBuffer::~DataBuffer()
{
    stop();  // 确保停止

    // 确保定时器被删除
    if (m_timer) {
        m_timer->stop();
        delete m_timer;
        m_timer = nullptr;
    }
}

void DataBuffer::start()
{
    if (!m_isRunning) {
        m_isRunning = true;
        m_buffer.clear();
        m_timer->start();
        qDebug() << "数据缓冲区已启动";
    }
}

void DataBuffer::stop()
{
    if (m_isRunning) {
        qDebug() << "正在停止数据缓冲区...";
        m_isRunning = false;

        // 先停止定时器
        if (m_timer && m_timer->isActive()) {
            m_timer->stop();
            qDebug() << "停止缓冲区定时器";
        }

        // 清空缓冲区，不处理剩余数据
        {
            QMutexLocker locker(&m_mutex);
            m_buffer.clear();
            qDebug() << "清空数据缓冲区";
        }

        qDebug() << "数据缓冲区已完全停止";
    }
}

bool DataBuffer::isRunning() const
{
    return m_isRunning;
}

void DataBuffer::addTemperature(float value)
{
    QMutexLocker locker(&m_mutex);
    m_buffer.temperature.append(value);
}

void DataBuffer::addHumidity(float value)
{
    QMutexLocker locker(&m_mutex);
    m_buffer.humidity.append(value);
}

void DataBuffer::addPressure(float value)
{
    QMutexLocker locker(&m_mutex);
    m_buffer.pressure.append(value);
}

void DataBuffer::addDewPoint(float value)
{
    QMutexLocker locker(&m_mutex);
    m_buffer.dewPoint.append(value);
}

void DataBuffer::addAltitude(float value)
{
    QMutexLocker locker(&m_mutex);
    m_buffer.altitude.append(value);
}

void DataBuffer::addWaterContent(float value)
{
    QMutexLocker locker(&m_mutex);
    m_buffer.waterContent.append(value);
}

void DataBuffer::addWaterContentPercent(float value)
{
    QMutexLocker locker(&m_mutex);
    m_buffer.waterContentPercent.append(value);
}

void DataBuffer::addWindSpeed(float value)
{
    QMutexLocker locker(&m_mutex);
    m_buffer.windSpeed.append(value);
}

void DataBuffer::addWindDirection(int angle, const QString& directionStr)
{
    QMutexLocker locker(&m_mutex);
    m_buffer.windDirectionAngles.append(angle);
    m_buffer.windDirectionStrs.append(directionStr);
}

void DataBuffer::processBuffer()
{
    QMutexLocker locker(&m_mutex);

    // 如果缓冲区已满（60秒数据），计算平均值
    if (m_buffer.isFull()) {
        WeatherData averagedData;
        calculateAverages(averagedData);

        // 发射信号
        emit dataReady(averagedData);

        emit dataStored();

        // 清空缓冲区
        m_buffer.clear();
    }
}

void DataBuffer::calculateAverages(WeatherData& result)
{
    result.timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    if (!m_buffer.temperature.isEmpty()) {
        result.temperature = calculateAverage(m_buffer.temperature);
    }

    if (!m_buffer.humidity.isEmpty()) {
        result.humidity = calculateAverage(m_buffer.humidity);
    }

    if (!m_buffer.pressure.isEmpty()) {
        result.pressure = calculateAverage(m_buffer.pressure);
    }

    if (!m_buffer.dewPoint.isEmpty()) {
        result.dewPoint = calculateAverage(m_buffer.dewPoint);
    }

    if (!m_buffer.altitude.isEmpty()) {
        result.altitude = calculateAverage(m_buffer.altitude);
    }

    if (!m_buffer.waterContent.isEmpty()) {
        result.waterContent = calculateAverage(m_buffer.waterContent);
    }

    if (!m_buffer.waterContentPercent.isEmpty()) {
        result.waterContentPercent = calculateAverage(m_buffer.waterContentPercent);
    }

    if (!m_buffer.windSpeed.isEmpty()) {
        result.windSpeed = calculateAverage(m_buffer.windSpeed);
    }

    if (!m_buffer.windDirectionAngles.isEmpty()) {
        result.windDirectionAngle = calculateWindDirectionAverage(
            m_buffer.windDirectionAngles, m_buffer.windDirectionStrs);
        result.windDirectionStr = getWindDirectionStr(result.windDirectionAngle);
    }
}

float DataBuffer::calculateAverage(const QVector<float>& values)
{
    if (values.isEmpty()) return 0;

    float sum = 0;
    for (float value : values) {
        sum += value;
    }
    return sum / values.size();
}

int DataBuffer::calculateWindDirectionAverage(const QVector<int>& angles, const QVector<QString>& directions)
{
    if (angles.isEmpty()) return 0;

    // 处理风向角度的循环特性（0°和360°相同）
    float sinSum = 0, cosSum = 0;

    for (int angle : angles) {
        float radian = angle * M_PI / 180.0;
        sinSum += sin(radian);
        cosSum += cos(radian);
    }

    float avgSin = sinSum / angles.size();
    float avgCos = cosSum / angles.size();

    int averageAngle = static_cast<int>(atan2(avgSin, avgCos) * 180.0 / M_PI);
    if (averageAngle < 0) averageAngle += 360;

    return averageAngle;
}

QString DataBuffer::getWindDirectionStr(int angle)
{
    if (angle >= 337.5 || angle < 22.5) return "北风";
    else if (angle >= 22.5 && angle < 67.5) return "东北风";
    else if (angle >= 67.5 && angle < 112.5) return "东风";
    else if (angle >= 112.5 && angle < 157.5) return "东南风";
    else if (angle >= 157.5 && angle < 202.5) return "南风";
    else if (angle >= 202.5 && angle < 247.5) return "西南风";
    else if (angle >= 247.5 && angle < 292.5) return "西风";
    else return "西北风";
}
