#ifndef MODBUSMANAGER_H
#define MODBUSMANAGER_H

#include <QObject>
#include <QTimer>
#include <QSerialPort>
#include <QModbusRtuSerialClient>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QThread>
#include <QEventLoop>
#include <QCoreApplication>

class ModbusManager : public QObject
{
    Q_OBJECT

public:
    explicit ModbusManager(QObject *parent = nullptr);
    ~ModbusManager();

    bool connectDevice(const QString &portName, int baudRate);
    void disconnectDevice();
    void sendReadCommand(quint8 deviceAddress = 1, quint16 startRegister = 0x0064, quint16 registerCount = 14);

    // 风速风向读取命令
    void sendReadWindSpeedCommand(quint8 deviceAddress = 2);
    void sendReadWindDirectionCommand(quint8 deviceAddress = 3);

    bool isConnected() const;
    void setContinuousReading(bool enable);  // 设置连续读取
    void stopContinuousReading();            // 停止连续读取

signals:
    void stateChanged(QModbusDevice::State state);
    void errorOccurred(QModbusDevice::Error error);
    void dataReceived(const QModbusDataUnit &unit);
    void rawDataReceived(const QByteArray &data);

    // 风速风向原始数据信号
    void windSpeedDataReceived(const QByteArray &data, quint8 deviceAddress);
    void windDirectionDataReceived(const QByteArray &data, quint8 deviceAddress);

private slots:
    void onModbusStateChanged(QModbusDevice::State state);
    void onModbusErrorOccurred(QModbusDevice::Error error);
    void onReadReady();
    void onTimeout();
    void onContinuousReadTimeout();  // 连续读取定时器

private:
    QModbusRtuSerialClient *m_modbusDevice;
    QTimer *m_readTimer;
    QTimer *m_continuousTimer;  // 连续读取定时器
    QModbusReply *m_currentReply;
    bool m_continuousReading;   // 是否连续读取
    int m_readCycleIndex;       // 读取周期索引

    quint8 m_lastDeviceAddress;  // 记录最后一次请求的设备地址
    QModbusDataUnit m_lastRequest;  // 记录最后一次请求

    // 解析原始响应，判断设备类型
    //void processRawResponse(const QByteArray &rawData);
};

#endif // MODBUSMANAGER_H
