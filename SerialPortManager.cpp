#include "SerialPortManager.h"
#include <QDebug>

SerialPortManager::SerialPortManager(QComboBox *serialComboBox, QObject *parent)
    : QObject(parent)
    , m_serialComboBox(serialComboBox)
    , m_portScanTimer(new QTimer(this))
    , m_isMonitoring(false)
{
    if (!m_serialComboBox) {
        qWarning() << "SerialPortManager: Invalid combo box provided";
        return;
    }

    // 连接端口选择变化信号
    connect(m_serialComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SerialPortManager::onPortSelectionChanged);

    // 设置定时器
    m_portScanTimer->setInterval(2000);
    m_portScanTimer->setSingleShot(false);
    connect(m_portScanTimer, &QTimer::timeout, this, &SerialPortManager::scanSerialPorts);
}

SerialPortManager::~SerialPortManager()
{
    stopMonitoring();
}

void SerialPortManager::startMonitoring()
{
    if (!m_serialComboBox) return;

    // 初始扫描
    scanSerialPorts();

    // 启动定时器
    m_portScanTimer->start();
    m_isMonitoring = true;
    qDebug() << "串口热插拔监控已启动，间隔: 2000ms";
}

void SerialPortManager::stopMonitoring()
{
    if (m_portScanTimer->isActive()) {
        m_portScanTimer->stop();
        m_isMonitoring = false;
        qDebug() << "串口热插拔监控已停止";
    }
}

QString SerialPortManager::getSelectedPortName() const
{
    if (!m_serialComboBox) return QString();

    QString displayText = m_serialComboBox->currentText();
    if (displayText.isEmpty() || displayText == "未找到串口") {
        return QString();
    }
    return extractPortName(displayText);
}

QString SerialPortManager::getDisplayText() const
{
    if (!m_serialComboBox) return QString();
    return m_serialComboBox->currentText();
}

void SerialPortManager::scanSerialPorts()
{
    if (!m_serialComboBox) return;

    QStringList currentPorts = getAvailablePorts();

    if (currentPorts != m_lastPortList) {
        qDebug() << "串口列表发生变化:";
        qDebug() << "  之前:" << m_lastPortList;
        qDebug() << "  现在:" << currentPorts;

        m_lastPortList = currentPorts;
        updatePortComboBox();
        emit portsUpdated();
    }
}

void SerialPortManager::onPortSelectionChanged(int index)
{
    Q_UNUSED(index);

    QString portName = getSelectedPortName();
    if (!portName.isEmpty()) {
        emit portSelected(portName);
    }
}

QStringList SerialPortManager::getAvailablePorts() const
{
    QStringList ports;

    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        if (isValidSerialPort(info)) {
            QString portName = info.portName();
            QString description = info.description();

            QString displayText;
            if (!description.isEmpty()) {
                displayText = QString("%1 (%2)").arg(portName, description);
            } else {
                displayText = portName;
            }

            ports << displayText;

            if (!isPortInList(portName)) {
                qDebug() << "发现新串口:" << portName << "描述:" << description;
            }
        }
    }

    return ports;
}

bool SerialPortManager::isValidSerialPort(const QSerialPortInfo &info) const
{
    Q_UNUSED(info);
    return true;
}

bool SerialPortManager::isPortInList(const QString &portName) const
{
    foreach (const QString &port, m_lastPortList) {
        if (port.contains(portName))
            return true;
    }
    return false;
}

QString SerialPortManager::extractPortName(const QString &displayText) const
{
    if (displayText.isEmpty())
        return QString();

    int spaceIndex = displayText.indexOf(' ');
    int parenIndex = displayText.indexOf('(');

    int endIndex = displayText.length();
    if (spaceIndex != -1 && spaceIndex < endIndex)
        endIndex = spaceIndex;
    if (parenIndex != -1 && parenIndex < endIndex)
        endIndex = parenIndex;

    return displayText.left(endIndex).trimmed();
}

void SerialPortManager::updatePortComboBox()
{
    if (!m_serialComboBox) return;

    QStringList currentPorts = getAvailablePorts();
    QString currentSelection = m_serialComboBox->currentText();
    QString currentPortName = extractPortName(currentSelection);

    m_serialComboBox->clear();

    if (currentPorts.isEmpty()) {
        m_serialComboBox->addItem("未找到串口");
        qDebug() << "更新下拉框：无可用串口";
    } else {
        m_serialComboBox->addItems(currentPorts);

        if (!currentPortName.isEmpty()) {
            bool restored = false;
            for (int i = 0; i < currentPorts.size(); i++) {
                if (extractPortName(currentPorts[i]) == currentPortName) {
                    m_serialComboBox->setCurrentIndex(i);
                    qDebug() << "成功恢复选中串口:" << currentPorts[i];
                    restored = true;
                    break;
                }
            }

            if (!restored && !currentPorts.isEmpty()) {
                m_serialComboBox->setCurrentIndex(0);
                qDebug() << "之前的串口已不可用，自动选择第一个:" << currentPorts[0];
            }
        } else if (!currentPorts.isEmpty()) {
            m_serialComboBox->setCurrentIndex(0);
        }

        qDebug() << "更新下拉框：共" << currentPorts.size() << "个串口";
    }
}
