#ifndef AUTOMATICWEATHER_1_H
#define AUTOMATICWEATHER_1_H

#include <QWidget>
#include "SerialPortManager.h"
#include "ModbusManager.h"
#include "SensorDataParser.h"
#include "DataBuffer.h"
#include "ExportDialog.h"

#include <QTableWidget>
#include <QTreeWidgetItem>
#include "CompassWidget.h"

#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include <QHeaderView>
#include <QFileDialog>
#include <QProgressDialog>
#include <QDateTime>
#include <QDateEdit>
#include <QProgressBar>
#include <QRandomGenerator>
#include <QStringConverter>

#include <QProgressBar>

QT_BEGIN_NAMESPACE
namespace Ui {
class AutomaticWeather_1;
}
QT_END_NAMESPACE

class AutomaticWeather_1 : public QWidget
{
    Q_OBJECT

public:
    AutomaticWeather_1(QWidget *parent = nullptr);
    ~AutomaticWeather_1();

private slots:
    void on_open_clicked();            // 同一个按钮实现打开/关闭功能
    void onBaudRateChanged(int index);

    // Modbus信号处理
    void onModbusStateChanged(QModbusDevice::State state);
    void onModbusErrorOccurred(QModbusDevice::Error error);
    void onDataReceived(const QModbusDataUnit &unit);
    void onRawDataReceived(const QByteArray &rawData);  // 只用于温湿度设备

    // 风速风向数据信号
    void onWindSpeedReceived(float speed);
    void onWindDirectionReceived(int angle, int directionCode, const QString &directionStr);

    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    //void onSaveButtonClicked();
    //void onDatabaseExportFinished(bool success, const QString& message);
    void setupTableProperties();

    void on_save_clicked();

    void on_temp_clicked();

    void on_RH_2_clicked();

    void on_pressure_clicked();

    void on_water_clicked();

    void on_water_percent_clicked();

    void on_dewPoint_clicked();

private:
    Ui::AutomaticWeather_1 *ui;

    // 模块化对象
    SerialPortManager *m_serialPortManager;
    ModbusManager     *m_modbusManager;
    SensorDataParser  *m_sensorDataParser;
    DataBuffer        *m_dataBuffer;       // 数据缓冲区，每60次保存一次
    QTableWidget      *m_dataTable;        // 使用UI中的QTableWidget
 //   CompassWidget     *m_compassWidget;
    QTimer* m_treeUpdateTimer;             // 用于更新树控件的定时器
    QSet<QString> m_treeDates;             // 存储已经添加到树控件的日期


    float parseFloat32(const uint16_t *data);
    int m_currentDataType = -1;
    static const int MAX_DISPLAY_DAYS = 7;


    void showRealTimeData();                // 显示实时数据
    void setupDataTable();
    void setupCompassWidget();
    void loadDataForDate(const QDate& date);
    bool exportDataToCSV(const QDate& startDate, const QDate& endDate,
                         const QString& filePath, QProgressBar* progressBar);
    void parseAndUpdateSensorData(const QModbusDataUnit &unit);

    void updateTreeWidget();
    void updateTreeWithCurrentData();       // 更新树控件显示当前数据
    void addDateToTree(const QDate& date);  // 添加日期到树控件
    void removeOldDatesFromTree();          // 移除过旧的日期

    void onAveragedDataReady(const WeatherData& averagedData);
    bool m_isConnected;                     // 连接状态标志
    void updateDataCountInTree(const QString& timestamp);

    void initDatabase();
    void refreshChart();
    void updateChartDisplay();
    void switchDataType(int dataType);

protected:
    void closeEvent(QCloseEvent *event) override;
};

#endif // AUTOMATICWEATHER_1_H
