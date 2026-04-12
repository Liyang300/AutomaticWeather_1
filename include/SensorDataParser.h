#ifndef SENSORDATAPARSER_H
#define SENSORDATAPARSER_H

#include <QObject>
#include <QModbusDataUnit>

class SensorDataParser : public QObject
{
    Q_OBJECT

public:
    explicit SensorDataParser(QObject *parent = nullptr);

    void parseSensorData(const QModbusDataUnit &unit);
    void parseRawSensorData(const QByteArray &rawData);

    // 风速风向解析
    void parseWindSpeedData(const QByteArray &rawData, quint8 deviceAddress);
    void parseWindDirectionData(const QByteArray &rawData, quint8 deviceAddress);

signals:
    // 风速风向解析结果信号
    void windSpeedParsed(float speed);
    void windDirectionParsed(int angle, int directionCode, const QString &directionStr);

private:
    float parseFloat32(const uint16_t *data);
};

#endif // SENSORDATAPARSER_H
