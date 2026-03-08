#ifndef SERIALPORTMANAGER_H
#define SERIALPORTMANAGER_H

#include <QObject>
#include <QTimer>
#include <QSerialPortInfo>
#include <QComboBox>

class SerialPortManager : public QObject
{
    Q_OBJECT

public:
    explicit SerialPortManager(QComboBox *serialComboBox, QObject *parent = nullptr);
    ~SerialPortManager();

    void startMonitoring();
    void stopMonitoring();
    QString getSelectedPortName() const;
    QString getDisplayText() const;

signals:
    void portsUpdated();
    void portSelected(const QString &portName);

private slots:
    void scanSerialPorts();
    void onPortSelectionChanged(int index);

private:
    QComboBox *m_serialComboBox;
    QTimer *m_portScanTimer;
    QStringList m_lastPortList;
    bool m_isMonitoring;

    QStringList getAvailablePorts() const;
    bool isValidSerialPort(const QSerialPortInfo &info) const;
    bool isPortInList(const QString &portName) const;
    QString extractPortName(const QString &displayText) const;
    void updatePortComboBox();
};

#endif // SERIALPORTMANAGER_H
