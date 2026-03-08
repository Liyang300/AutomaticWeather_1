#include "SensorDataParser.h"
#include <QDebug>
#include <cmath>

SensorDataParser::SensorDataParser(QObject *parent)
    : QObject(parent)
{
}

void SensorDataParser::parseSensorData(const QModbusDataUnit &unit)
{
    if (unit.valueCount() < 14) {
        qDebug() << "错误：数据长度不足，需要14个寄存器，收到" << unit.valueCount();
        return;
    }

    qDebug() << "=== 温湿度传感器数据 ===";

    // 打印原始寄存器值（调试用）
    qDebug() << "寄存器原始值（十六进制）：";
    for (int i = 0; i < unit.valueCount() && i < 14; i++) {
        quint16 regValue = unit.value(i);
        qDebug() << QString("  Reg[%1] = 0x%2")
                        .arg(i + 100)
                        .arg(regValue, 4, 16, QChar('0')).toUpper();
    }

    qDebug() << "解析结果：";

    // 解析所有参数
    QStringList paramNames = {"温度", "湿度", "大气压", "露点温度", "海拔", "含水量", "含水量百分比"};
    QStringList units = {"°C", "%RH", "kPa", "°C", "m", "mg/m³", "%"};

    for (int i = 0; i < 7; i++) {
        if (i * 2 + 1 < unit.valueCount()) {
            // 获取两个寄存器的值
            quint16 highByte = unit.value(i * 2);
            quint16 lowByte = unit.value(i * 2 + 1);

            // 创建数组并传入地址
            uint16_t data[2] = {highByte, lowByte};
            float value = parseFloat32(data);

            // 只对含水量百分比进行转换（乘以100）
            if (i == 6) {
                value *= 100.0f;
            }

            QString message = QString("  %1: %2 %3")
                                  .arg(paramNames[i])
                                  .arg(value, 0, 'f', 2)
                                  .arg(units[i]);

            qDebug() << message;

            // 显示原始数据用于调试
            qDebug() << QString("    原始: Reg[%1]=0x%2, Reg[%3]=0x%4")
                            .arg(i * 2 + 100)
                            .arg(highByte, 4, 16, QChar('0')).toUpper()
                            .arg(i * 2 + 101)
                            .arg(lowByte, 4, 16, QChar('0')).toUpper();
        }
    }

    qDebug() << "==================";
}

void SensorDataParser::parseRawSensorData(const QByteArray &rawData)
{
    qDebug() << "=== 手动解析温湿度原始数据 ===";

    if (rawData.isEmpty()) {
        qDebug() << "错误：原始数据为空";
        return;
    }

    qDebug() << "原始数据(十六进制):" << rawData.toHex(' ').toUpper();
    qDebug() << "数据长度:" << rawData.size() << "字节";

    // 跳过可能的设备地址和功能码
    int dataStart = 0;
    int dataSize = rawData.size();

    // 尝试根据数据长度判断格式
    if (dataSize >= 29 && dataSize <= 33) {
        // 温湿度数据通常是7个浮点数（28字节）
        // 查找可能的起始位置
        if (dataSize == 33) {
            dataStart = 3; // 跳过地址(1)、功能码(1)、字节数(1)
        } else if (dataSize == 31) {
            dataStart = 2; // 跳过地址和功能码
        } else if (dataSize == 29) {
            dataStart = 1; // 跳过字节数
        }

        // 读取7个浮点数（每个4字节）
        for (int i = 0; i < 7 && dataStart + i*4 + 3 < rawData.size(); i++) {
            // 提取4字节浮点数
            uint8_t bytes[4];
            for (int j = 0; j < 4; j++) {
                bytes[j] = static_cast<uint8_t>(rawData[dataStart + i*4 + j]);
            }

            // 转换为浮点数（大端序）
            union {
                uint32_t i;
                float f;
            } converter;

            converter.i = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];

            QStringList paramNames = {"温度", "湿度", "大气压", "露点温度",
                                      "海拔", "含水量", "含水量百分比"};
            QStringList units = {"°C", "%RH", "kPa", "°C", "m", "g/m³", "%"};

            float value = converter.f;
            if (i == 6) {     // 含水量百分比
                value *= 100; // 转换为百分比
            }

            qDebug() << QString("  %1: %2 %3")
                            .arg(paramNames[i])
                            .arg(value, 0, 'f', 4)
                            .arg(units[i]);

            // 显示原始字节
            qDebug() << QString("    原始字节: %1 %2 %3 %4")
                            .arg(bytes[0], 2, 16, QChar('0')).toUpper()
                            .arg(bytes[1], 2, 16, QChar('0')).toUpper()
                            .arg(bytes[2], 2, 16, QChar('0')).toUpper()
                            .arg(bytes[3], 2, 16, QChar('0')).toUpper();
        }
    } else {
        qDebug() << "数据格式不符合温湿度数据特征";
    }
}

// 修改：解析风速数据
void SensorDataParser::parseWindSpeedData(const QByteArray &rawData, quint8 deviceAddress)
{
    Q_UNUSED(deviceAddress);

    qDebug() << "=== 解析风速数据 ===";

    if (rawData.isEmpty()) {
        qDebug() << "错误：风速数据为空";
        return;
    }

    qDebug() << "原始数据(十六进制):" << rawData.toHex(' ').toUpper();
    qDebug() << "数据长度:" << rawData.size() << "字节";

    // 风速数据格式：字节数（1字节）+ 风速值（2字节）
    if (rawData.size() < 3) {
        qDebug() << "错误：风速数据长度不足，至少需要3字节";
        return;
    }

    // 检查字节数
    quint8 byteCount = static_cast<quint8>(rawData[0]);
    if (byteCount != 0x02) {
        qDebug() << "警告：风速数据字节数不是0x02，实际为:" << QString("0x%1").arg(byteCount, 2, 16, QChar('0')).toUpper();
    }

    // 解析风速值（两个字节，高字节在前）
    quint16 windSpeedRaw = (static_cast<quint8>(rawData[1]) << 8) | static_cast<quint8>(rawData[2]);
    float windSpeed = windSpeedRaw / 10.0f; // 除以10得到实际风速（m/s）

    qDebug() << "风速数据解析:";
    qDebug() << "  字节数: 0x" << QString("%1").arg(byteCount, 2, 16, QChar('0')).toUpper();
    qDebug() << "  原始值: 0x" << QString("%1").arg(windSpeedRaw, 4, 16, QChar('0')).toUpper();
    qDebug() << "  十进制:" << windSpeedRaw;
    qDebug() << "  风速: " << QString::number(windSpeed, 'f', 1) << "m/s";
    qDebug() << "==================";

    emit windSpeedParsed(windSpeed);
}

// 修改：解析风向数据
// 修改：解析风向数据（8方位编码）
void SensorDataParser::parseWindDirectionData(const QByteArray &rawData, quint8 deviceAddress)
{
    Q_UNUSED(deviceAddress);

    qDebug() << "=== 解析风向数据 ===";

    if (rawData.isEmpty()) {
        qDebug() << "错误：风向数据为空";
        return;
    }

    qDebug() << "原始数据(十六进制):" << rawData.toHex(' ').toUpper();
    qDebug() << "数据长度:" << rawData.size() << "字节";

    // 风向数据格式：字节数（1字节）+ 方位编码（2字节）+ 整数角度值（2字节）
    if (rawData.size() < 5) {
        qDebug() << "错误：风向数据长度不足，至少需要5字节";
        return;
    }

    // 检查字节数
    quint8 byteCount = static_cast<quint8>(rawData[0]);
    if (byteCount != 0x04) {
        qDebug() << "警告：风向数据字节数不是0x04，实际为:" << QString("0x%1").arg(byteCount, 2, 16, QChar('0')).toUpper();
    }

    // 解析方位编码（8方位，0-7）
    quint16 directionCode = (static_cast<quint8>(rawData[1]) << 8) | static_cast<quint8>(rawData[2]);

    // 解析整数角度值（0-359度）
    quint16 intAngleRaw = (static_cast<quint8>(rawData[3]) << 8) | static_cast<quint8>(rawData[4]);
    int intAngle = intAngleRaw;

    // 将方位编码转换为方向和角度
    QString directionStr;
    int directionAngle = 0;

    switch (directionCode) {
    case 0x0000:  // 北风
        directionStr = "北风";
        directionAngle = 0;
        break;
    case 0x0001:  // 东北风
        directionStr = "东北风";
        directionAngle = 45;
        break;
    case 0x0002:  // 东风
        directionStr = "东风";
        directionAngle = 90;
        break;
    case 0x0003:  // 东南风
        directionStr = "东南风";
        directionAngle = 135;
        break;
    case 0x0004:  // 南风
        directionStr = "南风";
        directionAngle = 180;
        break;
    case 0x0005:  // 西南风
        directionStr = "西南风";
        directionAngle = 225;
        break;
    case 0x0006:  // 西风
        directionStr = "西风";
        directionAngle = 270;
        break;
    case 0x0007:  // 西北风
        directionStr = "西北风";
        directionAngle = 315;
        break;
    default:
        directionStr = "未知";
        directionAngle = intAngle;  // 如果编码未知，使用整数角度值
        qDebug() << "警告：未知的方向编码:" << QString("0x%1").arg(directionCode, 4, 16, QChar('0')).toUpper();
        break;
    }

    qDebug() << "风向数据解析:";
    qDebug() << "  字节数: 0x" << QString("%1").arg(byteCount, 2, 16, QChar('0')).toUpper();
    qDebug() << "  方位编码: 0x" << QString("%1").arg(directionCode, 4, 16, QChar('0')).toUpper();
    qDebug() << "  方位十进制:" << directionCode;
    qDebug() << "  风向描述:" << directionStr;
    qDebug() << "  方位角度:" << directionAngle << "度";
    qDebug() << "  整数角度原始值: 0x" << QString("%1").arg(intAngleRaw, 4, 16, QChar('0')).toUpper();
    qDebug() << "  整数角度十进制:" << intAngleRaw;
    qDebug() << "  整数角度值:" << intAngle << "度";
    qDebug() << "==================";

    // 发送解析结果：使用方位角度和描述
    emit windDirectionParsed(directionAngle, directionCode, directionStr);
}

float SensorDataParser::parseFloat32(const uint16_t *data)
{
    // 组合两个16位寄存器为32位浮点数（大端序）
    uint32_t combined = (static_cast<uint32_t>(data[0]) << 16) | data[1];
    float result;
    memcpy(&result, &combined, sizeof(result));

    // 检查是否为有效数字
    if (std::isnan(result) || std::isinf(result) || result > 1e10 || result < -1e10) {
        return 0.0f;
    }

    return result;
}
