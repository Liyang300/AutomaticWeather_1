#include "ModbusManager.h"
#include <QSerialPortInfo>
#include <QVariant>
#include <QDebug>

ModbusManager::ModbusManager(QObject *parent)
    : QObject(parent)
    , m_modbusDevice(nullptr)
    , m_readTimer(new QTimer(this))
    , m_continuousTimer(new QTimer(this))
    , m_currentReply(nullptr)
    , m_continuousReading(false)
    , m_readCycleIndex(0)
    , m_lastDeviceAddress(0)
{
    m_readTimer->setSingleShot(true);
    connect(m_readTimer, &QTimer::timeout, this, &ModbusManager::onTimeout);

    // 设置连续读取定时器（500ms间隔）
    m_continuousTimer->setInterval(500);
    connect(m_continuousTimer, &QTimer::timeout, this, &ModbusManager::onContinuousReadTimeout);
}

ModbusManager::~ModbusManager()
{
    qDebug() << "ModbusManager析构开始...";

    // 先断开设备
    disconnectDevice();

    // 清理定时器
    if (m_readTimer) {
        m_readTimer->stop();
        delete m_readTimer;
        m_readTimer = nullptr;
        qDebug() << "清理读取定时器";
    }

    if (m_continuousTimer) {
        m_continuousTimer->stop();
        delete m_continuousTimer;
        m_continuousTimer = nullptr;
        qDebug() << "清理连续读取定时器";
    }

    qDebug() << "ModbusManager析构完成";
}

bool ModbusManager::connectDevice(const QString &portName, int baudRate)
{
    qDebug() << "连接参数详情：";
    qDebug() << "纯端口名:" << portName;
    qDebug() << "波特率:" << baudRate;

    // 清理之前的设备
    disconnectDevice();

    m_modbusDevice = new QModbusRtuSerialClient(this);

    // 连接信号槽
    connect(m_modbusDevice, &QModbusClient::stateChanged,
            this, &ModbusManager::onModbusStateChanged);
    connect(m_modbusDevice, &QModbusDevice::errorOccurred,
            this, &ModbusManager::onModbusErrorOccurred);

    // 设置串口参数
    qDebug() << "设置串口参数...";
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialPortNameParameter, portName);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialParityParameter, QSerialPort::NoParity);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, baudRate);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, QSerialPort::Data8);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, QSerialPort::OneStop);

    // 设置超时和重试
    int timeout = (baudRate <= 9600) ? 3000 : 2000;
    m_modbusDevice->setTimeout(timeout);
    m_modbusDevice->setNumberOfRetries(2);

    qDebug() << "正在连接设备...";
    return m_modbusDevice->connectDevice();
}

void ModbusManager::disconnectDevice()
{
    qDebug() << "正在断开Modbus设备...";

    // 1. 先停止连续读取
    if (m_continuousReading) {
        stopContinuousReading();
        qDebug() << "停止连续读取";
    }

    // 2. 停止连续读取定时器
    if (m_continuousTimer && m_continuousTimer->isActive()) {
        m_continuousTimer->stop();
        qDebug() << "停止连续读取定时器";
    }

    // 3. 停止读取超时定时器
    if (m_readTimer && m_readTimer->isActive()) {
        m_readTimer->stop();
        qDebug() << "停止读取超时定时器";
    }

    // 4. 清理当前回复
    if (m_currentReply) {
        if (!m_currentReply->isFinished()) {
            qDebug() << "有未完成的请求，正在清理...";
            // 断开信号连接，避免触发已完成信号
            m_currentReply->disconnect();

            // 不等待，直接标记为完成
            m_currentReply->deleteLater();
        } else {
            m_currentReply->deleteLater();
        }
        m_currentReply = nullptr;
        qDebug() << "清理当前回复";
    }

    // 5. 断开设备连接
    if (m_modbusDevice) {
        if (m_modbusDevice->state() != QModbusDevice::UnconnectedState) {
            // 异步断开，不等待
            m_modbusDevice->disconnectDevice();
            qDebug() << "发送断开连接请求";

            // 短暂延迟，让设备开始断开
            QCoreApplication::processEvents();
            QThread::msleep(50);
        }

        // 删除设备对象
        delete m_modbusDevice;
        m_modbusDevice = nullptr;
        qDebug() << "删除Modbus设备对象";
    }

    // 6. 重置状态
    m_continuousReading = false;
    m_readCycleIndex = 0;
    m_lastDeviceAddress = 0;

    qDebug() << "Modbus设备断开完成";
}

void ModbusManager::sendReadCommand(quint8 deviceAddress, quint16 startRegister, quint16 registerCount)
{
    if (!m_modbusDevice || m_modbusDevice->state() != QModbusDevice::ConnectedState) {
        qDebug() << "错误：设备未连接，无法发送命令";
        return;
    }

    // 创建读取请求
    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, startRegister, registerCount);

    // 保存请求信息
    m_lastDeviceAddress = deviceAddress;
    m_lastRequest = readUnit;

    qDebug() << "发送温湿度读取命令：";
    qDebug() << "  设备地址:" << deviceAddress;
    qDebug() << "  起始寄存器:" << QString("0x%1").arg(startRegister, 4, 16, QChar('0')).toUpper();
    qDebug() << "  寄存器数量:" << registerCount;

    // 发送请求
    m_currentReply = m_modbusDevice->sendReadRequest(readUnit, deviceAddress);
    if (!m_currentReply) {
        qDebug() << "发送请求失败：" << m_modbusDevice->errorString();
        return;
    }

    // 连接完成信号
    if (!m_currentReply->isFinished()) {
        connect(m_currentReply, &QModbusReply::finished, this, &ModbusManager::onReadReady);

        // 启动超时定时器
        m_readTimer->start(5000);
        qDebug() << "已设置超时时间: 5秒";
    } else {
        onReadReady();
    }
}

// 新增：发送风速读取命令
void ModbusManager::sendReadWindSpeedCommand(quint8 deviceAddress)
{
    if (!m_modbusDevice || m_modbusDevice->state() != QModbusDevice::ConnectedState) {
        qDebug() << "错误：设备未连接，无法发送风速命令";
        return;
    }

    // 风速读取命令: 0x02 0x03 0x00 0x00 0x00 0x01
    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, 0x0000, 1);

    // 保存请求信息
    m_lastDeviceAddress = deviceAddress;
    m_lastRequest = readUnit;

    qDebug() << "发送风速读取命令：";
    qDebug() << "  设备地址:" << deviceAddress;
    qDebug() << "  指令: 0x02 0x03 0x00 0x00 0x00 0x01";

    // 发送请求
    m_currentReply = m_modbusDevice->sendReadRequest(readUnit, deviceAddress);
    if (!m_currentReply) {
        qDebug() << "发送风速请求失败：" << m_modbusDevice->errorString();
        return;
    }

    // 连接完成信号
    if (!m_currentReply->isFinished()) {
        connect(m_currentReply, &QModbusReply::finished, this, &ModbusManager::onReadReady);

        // 启动超时定时器
        m_readTimer->start(2000); // 风速读取超时设置为2秒
    } else {
        onReadReady();
    }
}

// 新增：发送风向读取命令
void ModbusManager::sendReadWindDirectionCommand(quint8 deviceAddress)
{
    if (!m_modbusDevice || m_modbusDevice->state() != QModbusDevice::ConnectedState) {
        qDebug() << "错误：设备未连接，无法发送风向命令";
        return;
    }

    // 风向读取命令: 0x03 0x03 0x00 0x00 0x00 0x02
    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, 0x0000, 2);

    // 保存请求信息
    m_lastDeviceAddress = deviceAddress;
    m_lastRequest = readUnit;

    qDebug() << "发送风向读取命令：";
    qDebug() << "  设备地址:" << deviceAddress;
    qDebug() << "  指令: 0x03 0x03 0x00 0x00 0x00 0x02";

    // 发送请求
    m_currentReply = m_modbusDevice->sendReadRequest(readUnit, deviceAddress);
    if (!m_currentReply) {
        qDebug() << "发送风向请求失败：" << m_modbusDevice->errorString();
        return;
    }

    // 连接完成信号
    if (!m_currentReply->isFinished()) {
        connect(m_currentReply, &QModbusReply::finished, this, &ModbusManager::onReadReady);

        // 启动超时定时器
        m_readTimer->start(2000); // 风向读取超时设置为2秒
    } else {
        onReadReady();
    }
}

bool ModbusManager::isConnected() const
{
    return m_modbusDevice && m_modbusDevice->state() == QModbusDevice::ConnectedState;
}

// 新增：设置连续读取
void ModbusManager::setContinuousReading(bool enable)
{
    m_continuousReading = enable;
    if (enable) {
        m_readCycleIndex = 0;
        m_continuousTimer->start();
        qDebug() << "开始连续读取数据...";
    } else {
        stopContinuousReading();
    }
}

// 新增：停止连续读取
void ModbusManager::stopContinuousReading()
{
    m_continuousReading = false;
    if (m_continuousTimer->isActive()) {
        m_continuousTimer->stop();
        qDebug() << "停止连续读取数据";
    }
}

void ModbusManager::onModbusStateChanged(QModbusDevice::State state)
{
    emit stateChanged(state);

    if (state == QModbusDevice::ConnectedState) {
        qDebug() << "设备连接成功！";
        // 连接成功后开始连续读取
        setContinuousReading(true);
    } else if (state == QModbusDevice::UnconnectedState) {
        // 断开连接时停止连续读取
        stopContinuousReading();
    }
}

void ModbusManager::onModbusErrorOccurred(QModbusDevice::Error error)
{
    if (error == QModbusDevice::NoError) {
        return;
    }

    QString errorMsg;
    switch (error) {
    case QModbusDevice::ConnectionError:
        errorMsg = "连接错误";
        break;
    case QModbusDevice::TimeoutError:
        errorMsg = "响应超时 - 请检查波特率设置";
        break;
    case QModbusDevice::ProtocolError:
        errorMsg = "协议错误 - 请检查设备地址和功能码";
        break;
    case QModbusDevice::UnknownError:
        errorMsg = "未知错误";
        break;
    default:
        errorMsg = m_modbusDevice ? m_modbusDevice->errorString() : "未知错误";
        break;
    }

    qDebug() << "Modbus错误:" << errorMsg;

    // 清理当前请求
    if (m_currentReply) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    // 停止定时器
    if (m_readTimer->isActive()) {
        m_readTimer->stop();
    }

    emit errorOccurred(error);
}

void ModbusManager::onReadReady()
{
    if (m_readTimer->isActive()) {
        m_readTimer->stop();
    }

    auto *reply = qobject_cast<QModbusReply *>(sender());
    if (!reply) {
        if (m_currentReply) {
            reply = m_currentReply;
        } else {
            qDebug() << "错误：无效的回复对象";
            return;
        }
    }

    if (reply->error() != QModbusDevice::NoError) {
        qDebug() << "读取失败：" << reply->errorString();
        reply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    // 获取原始数据
    QModbusResponse response = reply->rawResult();
    QByteArray rawData = response.data();

    if (!rawData.isEmpty()) {
        // 根据设备地址决定发送哪个信号
        switch (m_lastDeviceAddress) {
        case 1:  // 温湿度设备
            emit rawDataReceived(rawData);
            break;
        case 2:  // 风速设备
            emit windSpeedDataReceived(rawData, m_lastDeviceAddress);
            break;
        case 3:  // 风向设备
            emit windDirectionDataReceived(rawData, m_lastDeviceAddress);
            break;
        default:
            // 其他设备也发送原始数据信号
            emit rawDataReceived(rawData);
            break;
        }
    }

    // 使用Qt Modbus解析的数据（主要用于温湿度）
    const QModbusDataUnit unit = reply->result();
    if (unit.isValid()) {
        // 温湿度设备发送dataReceived信号
        if (m_lastDeviceAddress == 1) {
            emit dataReceived(unit);
        }
    }

    reply->deleteLater();
    m_currentReply = nullptr;
}

void ModbusManager::onTimeout()
{
    qDebug() << "错误：读取超时！";
    qDebug() << "可能的原因：";
    qDebug() << "1. 设备未响应";
    qDebug() << "2. 波特率设置错误";
    qDebug() << "3. 设备地址错误";

    // 取消当前请求
    if (m_currentReply) {
        m_currentReply->disconnect();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
}

// 新增：连续读取定时器
void ModbusManager::onContinuousReadTimeout()
{
    if (!m_continuousReading || !m_modbusDevice ||
        m_modbusDevice->state() != QModbusDevice::ConnectedState) {
        return;
    }

    // 如果有未完成的请求，跳过本次
    if (m_currentReply && !m_currentReply->isFinished()) {
        return;
    }

    // 循环读取不同设备的数据
    switch (m_readCycleIndex) {
    case 0:
        // 读取温湿度数据
        sendReadCommand(1, 0x0064, 14);
        break;
    case 1:
        // 读取风速数据
        sendReadWindSpeedCommand(2);
        break;
    case 2:
        // 读取风向数据
        sendReadWindDirectionCommand(3);
        break;
    }

    // 更新索引，循环读取
    m_readCycleIndex = (m_readCycleIndex + 1) % 3;
}

// 处理原始响应，判断设备类型
/*void ModbusManager::processRawResponse(const QByteArray &rawData)
{
    if (rawData.isEmpty()) {
        return;
    }

    // 根据最后请求的设备地址分发数据
    switch (m_lastDeviceAddress) {
    case 2:  // 风速设备
        emit windSpeedDataReceived(rawData, m_lastDeviceAddress);
        break;
    case 3:  // 风向设备
        emit windDirectionDataReceived(rawData, m_lastDeviceAddress);
        break;
    // 温湿度设备的数据通过dataReceived信号处理
    default:
        break;
    }
}
*/
