// DataBuffer.h
#ifndef DATABUFFER_H
#define DATABUFFER_H

#include <QObject>
#include <QVector>
#include <QTimer>
#include <QMutex>
#include "DatabaseManager.h"

class DataBuffer : public QObject
{
    Q_OBJECT

public:
    explicit DataBuffer(QObject *parent = nullptr);
    ~DataBuffer();

    void start();
    void stop();
    bool isRunning() const;

    // 添加单个数据点
    void addTemperature(float value);
    void addHumidity(float value);
    void addPressure(float value);
    void addDewPoint(float value);
    void addAltitude(float value);
    void addWaterContent(float value);
    void addWaterContentPercent(float value);
    void addWindSpeed(float value);
    void addWindDirection(int angle, const QString& directionStr);

signals:
    void dataReady(const WeatherData& averagedData);

    void dataStored();

private slots:
    void processBuffer();

private:
    struct Buffer {
        QVector<float> temperature;
        QVector<float> humidity;
        QVector<float> pressure;
        QVector<float> dewPoint;
        QVector<float> altitude;
        QVector<float> waterContent;
        QVector<float> waterContentPercent;
        QVector<float> windSpeed;
        QVector<int> windDirectionAngles;
        QVector<QString> windDirectionStrs;

        void clear() {
            temperature.clear();
            humidity.clear();
            pressure.clear();
            dewPoint.clear();
            altitude.clear();
            waterContent.clear();
            waterContentPercent.clear();
            windSpeed.clear();
            windDirectionAngles.clear();
            windDirectionStrs.clear();
        }

        bool isFull() const {
            return temperature.size() >= 4;  // n个数据
        }

        bool isEmpty() const {
            return temperature.isEmpty();
        }
    };

    QTimer* m_timer;
    Buffer m_buffer;
    QMutex m_mutex;
    bool m_isRunning;

    void calculateAverages(WeatherData& result);
    float calculateAverage(const QVector<float>& values);
    int calculateWindDirectionAverage(const QVector<int>& angles, const QVector<QString>& directions);
    QString getWindDirectionStr(int angle);
};

#endif // DATABUFFER_H
