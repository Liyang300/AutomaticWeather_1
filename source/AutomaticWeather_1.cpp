#include "AutomaticWeather_1.h"
#include "./ui_AutomaticWeather_1.h"
#include "logger.h"


AutomaticWeather_1::AutomaticWeather_1(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AutomaticWeather_1)
    , m_serialPortManager(nullptr)
    , m_modbusManager    (nullptr)
    , m_sensorDataParser (nullptr)
    , m_isConnected      (false)
    , m_dataTable        (nullptr)
 //   , m_compassWidget    (nullptr)
    , m_treeUpdateTimer  (nullptr)
    , m_dataBuffer       (nullptr)
{
    ui->setupUi(this);

    initDatabase();

    // 初始化数据库管理器（确保数据库文件创建）
    LOG_INFO(u8"初始化数据库...");
    DatabaseManager& dbManager = DatabaseManager::instance();
    if (!dbManager.initialize()) {
        LOG_INFO("警告：数据库初始化失败，数据可能无法保存");
        QMessageBox::warning(this, "数据库错误",
                             "数据库初始化失败，数据将不会保存到文件。\n"
                             "错误信息：" + dbManager.databasePath());
    } else {
        qDebug() << "数据库初始化成功，文件路径：" << dbManager.databasePath();
    }

    // 初始化波特率选择下拉框
    QStringList baudRates;
    baudRates << "4800" << "9600" << "115200";
    ui->BaudRate->addItems(baudRates);

    // 默认选择9600
    int defaultIndex = baudRates.indexOf("9600");
    if (defaultIndex >= 0) {
        ui->BaudRate->setCurrentIndex(defaultIndex);
    }

    m_dataBuffer = new DataBuffer(this);
    connect(m_dataBuffer, &DataBuffer::dataReady, this, &AutomaticWeather_1::onAveragedDataReady);
    connect(m_dataBuffer, &DataBuffer::dataStored, this, &AutomaticWeather_1::refreshChart);
    refreshChart();

    // 连接波特率变化信号
    connect(ui->BaudRate, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AutomaticWeather_1::onBaudRateChanged);

    // 连接发送按钮信号
    connect(ui->sendButton, &QPushButton::clicked,
            this, &AutomaticWeather_1::on_pushButton_clicked);

    // 连接保存按钮信号
    connect(ui->save, &QPushButton::clicked,
            this, &AutomaticWeather_1::on_save_clicked);

    // 连接树控件点击信号
    connect(ui->tree, &QTreeWidget::itemClicked,
            this, &AutomaticWeather_1::onTreeItemClicked);

    // 初始化串口管理器
    m_serialPortManager = new SerialPortManager(ui->SerialCb, this);
    m_serialPortManager->startMonitoring();

    // 初始化Modbus管理器
    m_modbusManager = new ModbusManager(this);

    // 初始化数据解析器 - 保留原有解析器
    m_sensorDataParser = new SensorDataParser(this);

    // 连接Modbus信号
    connect(m_modbusManager, &ModbusManager::stateChanged,
            this, &AutomaticWeather_1::onModbusStateChanged);
    connect(m_modbusManager, &ModbusManager::errorOccurred,
            this, &AutomaticWeather_1::onModbusErrorOccurred);
    connect(m_modbusManager, &ModbusManager::dataReceived,
            this, &AutomaticWeather_1::onDataReceived);

    // 连接风速风向原始数据信号到解析器
    connect(m_modbusManager, &ModbusManager::windSpeedDataReceived,
            m_sensorDataParser, &SensorDataParser::parseWindSpeedData);
    connect(m_modbusManager, &ModbusManager::windDirectionDataReceived,
            m_sensorDataParser, &SensorDataParser::parseWindDirectionData);

    // 连接解析结果信号
    connect(m_sensorDataParser, &SensorDataParser::windSpeedParsed,
            this, &AutomaticWeather_1::onWindSpeedReceived);
    connect(m_sensorDataParser, &SensorDataParser::windDirectionParsed,
            this, &AutomaticWeather_1::onWindDirectionReceived);


    // 初始化表格
    setupDataTable();

    // 初始化罗盘
    setupCompassWidget();



    // 初始化树控件
    ui->tree->setColumnCount(1);
    ui->tree->setHeaderLabel("日期");
    updateTreeWidget();

    // 初始化按钮状态
    ui->open->setText("启动数据采集");

    // 初始化树控件更新定时器
    m_treeUpdateTimer = new QTimer(this);
    m_treeUpdateTimer->setInterval(1000);  // 每1秒更新一次树控件
    connect(m_treeUpdateTimer, &QTimer::timeout, this, &AutomaticWeather_1::updateTreeWithCurrentData);

    // 初始化树控件
    ui->tree->setColumnCount(2);  // 改为2列
    ui->tree->setHeaderLabels(QStringList() << "日期" << "数据条数");
    ui->tree->setColumnWidth(0, 120);  // 设置列宽
    ui->tree->setColumnWidth(1, 80);

    // 初始化按钮状态
    ui->open->setText("启动数据采集");

    //初始化按钮颜色
    switchDataType(0);


    qDebug() << "程序初始化完成";
}

AutomaticWeather_1::~AutomaticWeather_1()
{
    qDebug() << "开始析构，清理资源...";

    // 1. 首先停止所有定时器
    if (m_treeUpdateTimer) {
        qDebug() << "停止树控件更新定时器...";
        m_treeUpdateTimer->stop();
    }

    // 2. 停止数据缓冲区
    if (m_dataBuffer && m_dataBuffer->isRunning()) {
        qDebug() << "停止数据缓冲区...";
        m_dataBuffer->stop();
        // 等待一小段时间让缓冲区处理完剩余数据
        QThread::msleep(100);
    }

    // 3. 断开Modbus设备连接
    if (m_modbusManager) {
        qDebug() << "断开Modbus设备...";
        m_modbusManager->disconnectDevice();
        // 等待Modbus操作完成
        QThread::msleep(200);
    }

    // 4. 停止串口监控
    if (m_serialPortManager) {
        qDebug() << "停止串口监控...";
        m_serialPortManager->stopMonitoring();
    }

    // 5. 清理UI相关资源
    if (ui) {
        qDebug() << "清理UI资源...";
    }

    // 6. 按顺序删除对象
    delete m_treeUpdateTimer;
    m_treeUpdateTimer = nullptr;

    delete m_dataBuffer;
    m_dataBuffer = nullptr;

    delete m_modbusManager;
    m_modbusManager = nullptr;

    delete m_sensorDataParser;
    m_sensorDataParser = nullptr;

    delete m_serialPortManager;
    m_serialPortManager = nullptr;

 //   delete m_compassWidget;
 //   m_compassWidget = nullptr;

    delete m_dataTable;
    m_dataTable = nullptr;

    delete ui;
    ui = nullptr;

    qDebug() << "析构完成";
}

void AutomaticWeather_1::on_open_clicked()
{
    if (m_isConnected) {
        // 如果已连接，则断开连接
        qDebug() << "======================================";
        qDebug() << "断开设备连接...";

        // 1. 停止树控件更新定时器
        if (m_treeUpdateTimer && m_treeUpdateTimer->isActive()) {
            m_treeUpdateTimer->stop();
            qDebug() << "停止树控件更新";
        }

        // 2. 停止数据缓冲区
        if (m_dataBuffer && m_dataBuffer->isRunning()) {
            m_dataBuffer->stop();
            qDebug() << "停止数据缓冲区";

            // 短暂等待缓冲区停止
            QEventLoop loop;
            QTimer::singleShot(100, &loop, &QEventLoop::quit);
            loop.exec();
        }

        // 3. 断开Modbus设备
        if (m_modbusManager) {
            m_modbusManager->disconnectDevice();
            qDebug() << "断开Modbus设备";

            // 短暂等待设备断开
            QThread::msleep(100);
        }

        m_isConnected = false;
        ui->open->setText("启动数据采集");

        // 更新树控件显示
        updateTreeWidget();

        qDebug() << "设备已断开连接";
        qDebug() << "======================================";
        return;
    }

    qDebug() << "======================================";
    qDebug() << "开始连接传感器...";

    // 获取选择的串口和波特率
    QString portName = m_serialPortManager->getSelectedPortName();
    if (portName.isEmpty()) {
        qDebug() << "错误：未选择有效串口！";
        qDebug() << "请从下拉菜单中选择一个串口设备";
        QMessageBox::warning(this, "错误", "请选择有效的串口设备");
        return;
    }

    int baudRate = ui->BaudRate->currentText().toInt();

    qDebug() << "连接参数：";
    qDebug() << "  串口:" << portName;
    qDebug() << "  波特率:" << baudRate << "bps";
    qDebug() << "  温湿度设备地址: 1";
    qDebug() << "  风速设备地址: 2";
    qDebug() << "  风向设备地址: 3";

    // 连接设备
    if (m_modbusManager->connectDevice(portName, baudRate)) {
        qDebug() << "设备连接请求已发送，等待响应...";
        m_isConnected = true;
        ui->open->setText("关闭");

        // 清空之前的日期记录
        m_treeDates.clear();

        // 启动数据缓冲区
        if (m_dataBuffer) {
            if (!m_dataBuffer->isRunning()) {
                m_dataBuffer->start();
                qDebug() << "启动数据缓冲区（每60次保存一次）";
            } else {
                qDebug() << "数据缓冲区已在运行";
            }
        } else {
            qDebug() << "错误：数据缓冲区未初始化";
            QMessageBox::warning(this, "错误", "数据缓冲区初始化失败");
            return;
        }

        // 启动树控件更新定时器
        if (m_treeUpdateTimer) {
            if (!m_treeUpdateTimer->isActive()) {
                m_treeUpdateTimer->start();
                qDebug() << "启动树控件实时更新";
            }
        }

        qDebug() << "等待设备连接确认...";
    } else {
        qDebug() << "连接失败，请检查参数和设备状态";
        QMessageBox::warning(this, "错误", "连接失败，请检查参数和设备状态");
    }
    qDebug() << "======================================";
}

void AutomaticWeather_1::onBaudRateChanged(int index)
{
    QString baudRate = ui->BaudRate->itemText(index);
    qDebug() << "波特率更改为:" << baudRate << "bps";
}

void AutomaticWeather_1::on_pushButton_clicked()
{
    QString sendData = ui->sendshow->text();
    if (sendData.isEmpty()) {
        return;
    }

    qDebug() << "发送数据:" << sendData;
    // 这里可以添加发送逻辑
}

void AutomaticWeather_1::onModbusStateChanged(QModbusDevice::State state)
{
    QString stateStr;

    switch (state) {
    case QModbusDevice::UnconnectedState:
        stateStr = "未连接";
        m_isConnected = false;
        ui->open->setText("启动数据采集");
        qDebug() << "设备状态:" << stateStr;
        break;
    case QModbusDevice::ConnectingState:
        stateStr = "连接中...";
        qDebug() << "设备状态:" << stateStr;
        break;
    case QModbusDevice::ConnectedState:
        stateStr = "已连接";
        qDebug() << "设备状态:" << stateStr;
        qDebug() << "设备连接成功！";
        qDebug() << "开始连续读取传感器数据...";

        // 更新按钮状态
        m_isConnected = true;
        ui->open->setText("关闭串口");
        break;
    case QModbusDevice::ClosingState:
        stateStr = "断开中...";
        qDebug() << "设备状态:" << stateStr;
        break;
    }
}

void AutomaticWeather_1::onModbusErrorOccurred(QModbusDevice::Error error)
{
    Q_UNUSED(error);
    // 错误信息已经在ModbusManager中打印了
}

void AutomaticWeather_1::onDataReceived(const QModbusDataUnit &unit)
{
    qDebug() << "收到传感器数据，开始解析...";
    m_sensorDataParser->parseSensorData(unit);
    // 同时解析并更新LCDNum控件
    parseAndUpdateSensorData(unit);
}

void AutomaticWeather_1::onRawDataReceived(const QByteArray &rawData)
{
    qDebug() << "======================================";
    qDebug() << "温湿度设备原始响应数据（十六进制）：";
    QString hexString = rawData.toHex(' ').toUpper();
    qDebug() << "  " << hexString;
    qDebug() << "数据长度:" << rawData.size() << "字节";
    // m_sensorDataParser->parseRawSensorData(rawData);
}

// 风速数据接收
void AutomaticWeather_1::onWindSpeedReceived(float speed)
{
    qDebug() << "======================================";
    qDebug() << "风速数据接收:";
    qDebug() << "  风速:" << QString::number(speed, 'f', 1) << "m/s";

    // 更新UI显示风速到LCDNum控件
    if (ui->WinSpeed) {
        ui->WinSpeed->display(QString::number(speed, 'f', 1));
    }

    // 更新罗盘
    ui->compass->setWindSpeed(speed);
    ui->compass->setWindSpeed(speed);

    /*if (m_compassWidget) {
        m_compassWidget->setWindSpeed(speed);
        m_compassWidget->update();
    }*/

    // 添加到数据缓冲区
    if (m_dataBuffer && m_dataBuffer->isRunning()) {
        m_dataBuffer->addWindSpeed(speed);
        qDebug() << "风速数据已添加到缓冲区";
    }
}

// 风向数据接收
void AutomaticWeather_1::onWindDirectionReceived(int angle, int directionCode, const QString &directionStr)
{
    qDebug() << "======================================";
    qDebug() << "风向数据接收:";
    qDebug() << "  方位编码:" << directionCode;
    qDebug() << "  风向角度:" << angle << "度";
    qDebug() << "  风向描述:" << directionStr;

    // 更新罗盘
    ui->compass->setWindDirection(angle);
    ui->compass->setWindDirectionStr(directionStr);

    /*if (m_compassWidget) {
        m_compassWidget->setWindDirection(angle);
        m_compassWidget->setWindDirectionStr(directionStr);
        m_compassWidget->update();
    }*/

    // 添加到数据缓冲区
    if (m_dataBuffer && m_dataBuffer->isRunning()) {
        m_dataBuffer->addWindDirection(angle, directionStr);
        qDebug() << "风向数据已添加到缓冲区";
    }

    // 根据你的测试，显示具体方位
    QString debugInfo = QString("风向: %1 (%2度) [编码: 0x%3]")
                            .arg(directionStr)
                            .arg(angle)
                            .arg(directionCode, 4, 16, QChar('0')).toUpper();
    qDebug() << debugInfo;
}

void AutomaticWeather_1::setupDataTable()
{
    // 直接使用UI中已经存在的QTableWidget（对象名为dataTable）
    m_dataTable = ui->dataTable;  // 从UI中获取

    // 如果m_dataTable为空，说明UI中没有这个控件，需要创建
    if (!m_dataTable) {
        m_dataTable = new QTableWidget(this);
        m_dataTable->setObjectName("dataTable");
        m_dataTable->setGeometry(800, 10, 380, 580);
    }

    // 设置表格属性
    setupTableProperties();
}

void AutomaticWeather_1::setupTableProperties()
{
    if (!m_dataTable) return;

    // 设置列数和标题
    m_dataTable->setColumnCount(11);

    QStringList headers = {
        "时间", "温度(℃)", "湿度(%RH)", "大气压(kPa)",
        "露点温度(℃)", "海拔(m)", "含水量(mg/m³)",
        "含水量百分比(%)", "风速(m/s)", "风向角度", "风向描述"
    };

    m_dataTable->setHorizontalHeaderLabels(headers);

    // 设置表格属性
    m_dataTable->setAlternatingRowColors(true);
    m_dataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dataTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_dataTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 设置滚动条策略
    m_dataTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_dataTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // 设置列宽
    m_dataTable->horizontalHeader()->setStretchLastSection(true);
    m_dataTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);

    // 设置具体列宽
    m_dataTable->setColumnWidth(0, 130);  // 时间列宽一点
    for (int i = 1; i < 9; i++) {
        m_dataTable->setColumnWidth(i, 70);
    }
    m_dataTable->setColumnWidth(9, 80);   // 风向角度
    m_dataTable->setColumnWidth(10, 80); // 风向描述

    // 设置垂直头不可见
    m_dataTable->verticalHeader()->setVisible(false);

    // 启用排序
    m_dataTable->setSortingEnabled(true);
}

// parseAndUpdateSensorData 函数，只更新UI和添加到缓冲区
void AutomaticWeather_1::parseAndUpdateSensorData(const QModbusDataUnit &unit)
{
    if (unit.valueCount() < 14) {
        qDebug() << "数据长度不足，无法更新LCD显示";
        return;
    }

    // 解析温度
    float temperature = 0;
    if (unit.valueCount() >= 2) {
        uint16_t tempData[2] = {unit.value(0), unit.value(1)};
        temperature = parseFloat32(tempData);
        if (ui->Temp) {
            ui->Temp->display(QString::number(temperature, 'f', 1));
        }
        if (m_dataBuffer && m_dataBuffer->isRunning()) {
            m_dataBuffer->addTemperature(temperature);
        }
    }

    // 解析湿度
    float humidity = 0;
    if (unit.valueCount() >= 4) {
        uint16_t humidityData[2] = {unit.value(2), unit.value(3)};
        humidity = parseFloat32(humidityData);
        if (ui->RH) {
            ui->RH->display(QString::number(humidity, 'f', 1));
        }
        if (m_dataBuffer && m_dataBuffer->isRunning()) {
            m_dataBuffer->addHumidity(humidity);
        }
    }

    // 解析大气压
    float pressure = 0;
    if (unit.valueCount() >= 6) {
        uint16_t pressureData[2] = {unit.value(4), unit.value(5)};
        pressure = parseFloat32(pressureData);
        if (ui->hPa) {
            ui->hPa->display(QString::number(pressure, 'f', 2));
        }
        if (m_dataBuffer && m_dataBuffer->isRunning()) {
            m_dataBuffer->addPressure(pressure);
        }
    }

    // 解析露点温度
    float dewPoint = 0;
    if (unit.valueCount() >= 8) {
        uint16_t dewPointData[2] = {unit.value(6), unit.value(7)};
        dewPoint = parseFloat32(dewPointData);
        if (ui->DP_Temp) {
            ui->DP_Temp->display(QString::number(dewPoint, 'f', 1));
        }
        if (m_dataBuffer && m_dataBuffer->isRunning()) {
            m_dataBuffer->addDewPoint(dewPoint);
        }
    }

    // 解析海拔 - 直接使用原始值，不需要转换
    float altitude = 0;
    if (unit.valueCount() >= 10) {
        uint16_t altitudeData[2] = {unit.value(8), unit.value(9)};
        altitude = parseFloat32(altitudeData);

        if (ui->High) {
            ui->High->display(QString::number(altitude, 'f', 2));
        }
        if (m_dataBuffer && m_dataBuffer->isRunning()) {
            m_dataBuffer->addAltitude(altitude);
        }
    }

    // 解析含水量 - 直接使用原始值，不需要转换（单位已经是mg/m³）
    float waterContent = 0;
    if (unit.valueCount() >= 12) {
        uint16_t waterContentData[2] = {unit.value(10), unit.value(11)};
        waterContent = parseFloat32(waterContentData);

        // 注意：原始数据就是mg/m³，不需要任何转换
        if (ui->MC) {
            ui->MC->display(QString::number(waterContent, 'f', 2));
        }
        if (m_dataBuffer && m_dataBuffer->isRunning()) {
            m_dataBuffer->addWaterContent(waterContent);
        }
    }

    // 解析含水量百分比
    float waterContentPercent = 0;
    if (unit.valueCount() >= 14) {
        uint16_t waterPercentData[2] = {unit.value(12), unit.value(13)};
        waterContentPercent = parseFloat32(waterPercentData) * 100.0f;
        if (ui->MC_Percent) {
            ui->MC_Percent->display(QString::number(waterContentPercent, 'f', 2));
        }
        if (m_dataBuffer && m_dataBuffer->isRunning()) {
            m_dataBuffer->addWaterContentPercent(waterContentPercent);
        }
    }

    qDebug() << "数据已添加到缓冲区（每30次保存一次）";
}

// 辅助函数来解析浮点数：
float AutomaticWeather_1::parseFloat32(const uint16_t *data)
{
    uint32_t combined = (static_cast<uint32_t>(data[0]) << 16) | data[1];
    float result;
    memcpy(&result, &combined, sizeof(result));
    return result;
}

void AutomaticWeather_1::setupCompassWidget()
{
    /*
    // 在UI中创建一个QWidget容器用于放置罗盘
    // 可以动态创建

    QWidget* compassContainer = this->findChild<QWidget*>("compassContainer");
    if (compassContainer) {
        m_compassWidget = new CompassWidget(compassContainer);
        QVBoxLayout* layout = new QVBoxLayout(compassContainer);
        layout->addWidget(m_compassWidget);
        compassContainer->setLayout(layout);
    } else {
        // 如果UI中没有，创建一个新的
        // 修改这里的坐标和大小可以改变罗盘的位置和尺寸
        compassContainer = new QWidget(this);
        // setGeometry(x, y, width, height)
        compassContainer->setGeometry(750, 0, 180, 180);
        compassContainer->setObjectName("compassContainer");

        m_compassWidget = new CompassWidget(compassContainer);
        m_compassWidget->setGeometry(0, 0, 40, 180);
    }
    */
}

void AutomaticWeather_1::onTreeItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

    if (!item) {
        return;
    }

    QString dateStr = item->text(0);
    QDate date = QDate::fromString(dateStr, "yyyy-MM-dd");

    if (date.isValid())
    {
        /*
        // 检查是否点击了正在采集的日期
        if (date == QDate::currentDate() && m_isConnected)
            {
            int choice = QMessageBox::question(this, "查看数据",
                                               "这是今天的数据，采集仍在进行中。\n"
                                               "查看实时数据会中断采集吗？\n\n"
                                               "点击'是'查看实时表格数据\n"
                                               "点击'否'继续采集",
                                               QMessageBox::Yes | QMessageBox::No,
                                               QMessageBox::No);

            if (choice == QMessageBox::Yes)
            {
                // 从数据库加载数据并显示在表格中
                loadDataForDate(date);
            }
        }*/

        //else
        //{
            // 从数据库加载数据并显示在表格中
            loadDataForDate(date);
        //}
    }
}

//void AutomaticWeather_1::onSaveButtonClicked()
//{
//}

bool AutomaticWeather_1::exportDataToCSV(const QDate& startDate, const QDate& endDate,
                                         const QString& filePath, QProgressBar* progressBar)
{
    // 移除日期限制，允许导出任意日期
    // 只进行基本的日期有效性检查
    if (!startDate.isValid() || !endDate.isValid()) {
        qDebug() << "日期无效";
        return false;
    }

    if (startDate > endDate) {
        qDebug() << "开始日期不能晚于结束日期";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {  // 注意：这里去掉Text标志
        qDebug() << "无法打开文件:" << file.errorString();
        return false;
    }

    // 手动写入UTF-8 BOM
    QByteArray bom;
    bom.append(0xEF);
    bom.append(0xBB);
    bom.append(0xBF);
    file.write(bom);

    // 使用QTextStream写入文本
    QTextStream stream(&file);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif

    // 写入CSV头部
    QStringList headers = {
        "时间戳", "温度(℃)", "湿度(%RH)", "大气压(kPa)",
        "露点温度(℃)", "海拔(m)", "含水量(mg/m³)",  // 修正单位
        "含水量百分比(%)", "风速(m/s)", "风向角度", "风向描述"
    };

    stream << headers.join(",") << "\n";

    // 使用实际数据库查询数据
    DatabaseManager& dbManager = DatabaseManager::instance();
    QVector<WeatherData> data = dbManager.getDataByDateRange(startDate, endDate);

    if (data.isEmpty()) {
        qDebug() << "指定日期范围内没有数据";
        file.close();
        return false;
    }

    // 导出实际数据
    int totalSteps = data.size();
    for (int i = 0; i < totalSteps; i++) {
        const WeatherData& record = data[i];

        QString line = QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11")
                           .arg(record.timestamp)
                           .arg(record.temperature)
                           .arg(record.humidity)
                           .arg(record.pressure)
                           .arg(record.dewPoint)
                           .arg(record.altitude)
                           .arg(record.waterContent)          // 注意单位是mg/m³
                           .arg(record.waterContentPercent)
                           .arg(record.windSpeed)
                           .arg(record.windDirectionAngle)
                           .arg(record.windDirectionStr);

        stream << line << "\n";

        // 更新进度
        if (progressBar) {
            progressBar->setValue((i + 1) * 100 / totalSteps);
            QCoreApplication::processEvents();
        }
    }

    file.close();
    qDebug() << "成功导出" << totalSteps << "条数据到:" << filePath;
    return true;
}

void AutomaticWeather_1::loadDataForDate(const QDate& date)
{
    if (!m_dataTable) return;

    // 清空表格
    m_dataTable->setRowCount(0);

    // 检查是否是今天的数据
    if (date == QDate::currentDate() && m_isConnected) {
        // 显示实时数据
        showRealTimeData();
        return;
    }

    // 从数据库加载实际数据
    DatabaseManager& dbManager = DatabaseManager::instance();
    QVector<WeatherData> data = dbManager.getDataByDate(date);

    if (!data.isEmpty()) {
        // 使用索引循环完全避免迭代器问题
        for (int i = 0; i < data.size(); ++i) {
            const WeatherData& record = data.at(i);
            int row = m_dataTable->rowCount();
            m_dataTable->insertRow(row);

            m_dataTable->setItem(row, 0, new QTableWidgetItem(record.timestamp));
            m_dataTable->setItem(row, 1, new QTableWidgetItem(QString::number(record.temperature, 'f', 1)));
            m_dataTable->setItem(row, 2, new QTableWidgetItem(QString::number(record.humidity, 'f', 1)));
            m_dataTable->setItem(row, 3, new QTableWidgetItem(QString::number(record.pressure, 'f', 2)));
            m_dataTable->setItem(row, 4, new QTableWidgetItem(QString::number(record.dewPoint, 'f', 1)));
            m_dataTable->setItem(row, 5, new QTableWidgetItem(QString::number(record.altitude, 'f', 1)));
            m_dataTable->setItem(row, 6, new QTableWidgetItem(QString::number(record.waterContent, 'f', 2)));  // mg/m³
            m_dataTable->setItem(row, 7, new QTableWidgetItem(QString::number(record.waterContentPercent, 'f', 2)));
            m_dataTable->setItem(row, 8, new QTableWidgetItem(QString::number(record.windSpeed, 'f', 1)));
            m_dataTable->setItem(row, 9, new QTableWidgetItem(QString::number(record.windDirectionAngle)));
            m_dataTable->setItem(row, 10, new QTableWidgetItem(record.windDirectionStr));
        }
    } else {
        // 数据库中没有数据，显示提示
        int row = m_dataTable->rowCount();
        m_dataTable->insertRow(row);

        m_dataTable->setItem(row, 0, new QTableWidgetItem(date.toString("yyyy-MM-dd")));
        m_dataTable->setItem(row, 1, new QTableWidgetItem("无数据"));
        m_dataTable->setItem(row, 2, new QTableWidgetItem("无数据"));
        m_dataTable->setItem(row, 3, new QTableWidgetItem("无数据"));
        m_dataTable->setItem(row, 4, new QTableWidgetItem("无数据"));
        m_dataTable->setItem(row, 5, new QTableWidgetItem("无数据"));
        m_dataTable->setItem(row, 6, new QTableWidgetItem("无数据"));
        m_dataTable->setItem(row, 7, new QTableWidgetItem("无数据"));
        m_dataTable->setItem(row, 8, new QTableWidgetItem("无数据"));
        m_dataTable->setItem(row, 9, new QTableWidgetItem("无数据"));
        m_dataTable->setItem(row, 10, new QTableWidgetItem("无数据"));
    }

    // 滚动到顶部
    m_dataTable->scrollToTop();
}



void AutomaticWeather_1::showRealTimeData()
{
    // 清空表格
    m_dataTable->setRowCount(0);

    // 在表格中添加一行显示"实时采集中..."
    int row = m_dataTable->rowCount();
    m_dataTable->insertRow(row);

    QDateTime currentTime = QDateTime::currentDateTime();
    m_dataTable->setItem(row, 0, new QTableWidgetItem(currentTime.toString("yyyy-MM-dd HH:mm:ss")));
    m_dataTable->setItem(row, 1, new QTableWidgetItem("实时采集中..."));
    m_dataTable->setItem(row, 2, new QTableWidgetItem("实时采集中..."));
    m_dataTable->setItem(row, 3, new QTableWidgetItem("实时采集中..."));
    m_dataTable->setItem(row, 4, new QTableWidgetItem("实时采集中..."));
    m_dataTable->setItem(row, 5, new QTableWidgetItem("实时采集中..."));
    m_dataTable->setItem(row, 6, new QTableWidgetItem("实时采集中..."));
    m_dataTable->setItem(row, 7, new QTableWidgetItem("实时采集中..."));
    m_dataTable->setItem(row, 8, new QTableWidgetItem("实时采集中..."));
    m_dataTable->setItem(row, 9, new QTableWidgetItem("实时采集中..."));
    m_dataTable->setItem(row, 10, new QTableWidgetItem("实时采集中..."));

    // 设置特殊样式
    for (int col = 0; col < 11; col++) {
        QTableWidgetItem* item = m_dataTable->item(row, col);
        if (item) {
            item->setForeground(QBrush(Qt::red));
            item->setFont(QFont("Arial", 10, QFont::Bold));
        }
    }

    // 滚动到底部
    m_dataTable->scrollToBottom();
}

void AutomaticWeather_1::updateTreeWidget()
{
    // 清空树控件
    ui->tree->clear();
    m_treeDates.clear();

    // 从数据库获取所有已有数据的日期
    DatabaseManager& dbManager = DatabaseManager::instance();
    QVector<QDate> availableDates = dbManager.getAvailableDates();

    // 如果正在采集数据，确保今天在列表中
    if (m_isConnected) {
        QDate today = QDate::currentDate();
        if (!availableDates.contains(today)) {
            availableDates.append(today);
        }
    }

    // 对日期进行排序（最新的在前）
    std::sort(availableDates.begin(), availableDates.end(), std::greater<QDate>());

    // 修复：使用迭代器而不是范围循环，避免detach警告
    for (int i = 0; i < availableDates.size(); ++i) {
        const QDate& date = availableDates.at(i);
        QString dateStr = date.toString("yyyy-MM-dd");
        QTreeWidgetItem* item = new QTreeWidgetItem(ui->tree);
        item->setText(0, dateStr);

        // 从数据库获取该日期的数据条数
        QVector<WeatherData> data = dbManager.getDataByDate(date);
        if (data.isEmpty() && date == QDate::currentDate() && m_isConnected) {
            item->setText(1, "采集中...");
        } else {
            item->setText(1, QString::number(data.size()) + "条");
        }

        // 如果是今天的数据，设置为粗体
        if (date == QDate::currentDate()) {
            QFont font = item->font(0);
            font.setBold(true);
            item->setFont(0, font);
            item->setForeground(0, QBrush(Qt::red));
            item->setForeground(1, QBrush(Qt::red));
        }

        // 添加到树控件
        ui->tree->addTopLevelItem(item);
        m_treeDates.insert(dateStr);
    }

    // 如果没有任何日期，至少添加今天的日期
    if (availableDates.isEmpty() && m_isConnected) {
        addDateToTree(QDate::currentDate());
    }
}


void AutomaticWeather_1::on_save_clicked()
{
    ExportDialog dialog(this);

    connect(&dialog, &ExportDialog::exportRequested,
            this, [this, &dialog](const QDate& start, const QDate& end, const QString& path) {
                DatabaseManager& db = DatabaseManager::instance();
                connect(&db, &DatabaseManager::exportProgress,
                        &dialog, &ExportDialog::onExportProgress);
                connect(&db, &DatabaseManager::exportFinished,
                        &dialog, &ExportDialog::onExportFinished);
                db.exportToCSV(path, start, end);
            });

    dialog.exec();
    /*
    // 创建导出对话框
    QDialog exportDialog(this);
    exportDialog.setWindowTitle("导出数据");
    exportDialog.setMinimumSize(400, 200);

    QVBoxLayout* mainLayout = new QVBoxLayout(&exportDialog);

    // 日期选择
    QHBoxLayout* dateLayout = new QHBoxLayout();
    QLabel* startLabel = new QLabel("开始日期:", &exportDialog);
    QDateEdit* startDateEdit = new QDateEdit(&exportDialog);
    startDateEdit->setCalendarPopup(true);
    startDateEdit->setDate(QDate::currentDate().addDays(-7));

    QLabel* endLabel = new QLabel("结束日期:", &exportDialog);
    QDateEdit* endDateEdit = new QDateEdit(&exportDialog);
    endDateEdit->setCalendarPopup(true);
    endDateEdit->setDate(QDate::currentDate());

    dateLayout->addWidget(startLabel);
    dateLayout->addWidget(startDateEdit);
    dateLayout->addWidget(endLabel);
    dateLayout->addWidget(endDateEdit);

    // 文件路径选择
    QHBoxLayout* fileLayout = new QHBoxLayout();
    QLabel* fileLabel = new QLabel("保存路径:", &exportDialog);
    QLineEdit* filePathEdit = new QLineEdit(&exportDialog);
    QPushButton* browseButton = new QPushButton("浏览...", &exportDialog);

    QString defaultPath = QDir::currentPath() + "/weather_data_" +
                          QDate::currentDate().toString("yyyyMMdd") + ".csv";
    filePathEdit->setText(defaultPath);

    fileLayout->addWidget(fileLabel);
    fileLayout->addWidget(filePathEdit);
    fileLayout->addWidget(browseButton);

    // 进度条
    QProgressBar* progressBar = new QProgressBar(&exportDialog);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setVisible(false);

    // 按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* exportButton = new QPushButton("导出", &exportDialog);
    QPushButton* cancelButton = new QPushButton("取消", &exportDialog);

    buttonLayout->addStretch();
    buttonLayout->addWidget(exportButton);
    buttonLayout->addWidget(cancelButton);

    // 添加到主布局
    mainLayout->addLayout(dateLayout);
    mainLayout->addLayout(fileLayout);
    mainLayout->addWidget(progressBar);
    mainLayout->addLayout(buttonLayout);

    // 连接信号
    connect(browseButton, &QPushButton::clicked, [filePathEdit]() {
        QString fileName = QFileDialog::getSaveFileName(
            nullptr,
            "保存文件",
            QDir::currentPath(),
            "CSV文件 (*.csv);;所有文件 (*.*)"
            );

        if (!fileName.isEmpty()) {
            filePathEdit->setText(fileName);
        }
    });

    connect(cancelButton, &QPushButton::clicked, &exportDialog, &QDialog::reject);

    // 导出操作
    connect(exportButton, &QPushButton::clicked, [&]() {
        QDate startDate = startDateEdit->date();
        QDate endDate = endDateEdit->date();
        QString filePath = filePathEdit->text();

        if (startDate > endDate) {
            QMessageBox::warning(this, "错误", "开始日期不能晚于结束日期");
            return;
        }

        if (filePath.isEmpty()) {
            QMessageBox::warning(this, "错误", "请选择保存路径");
            return;
        }

        // 显示进度条
        progressBar->setVisible(true);
        exportButton->setEnabled(false);

        // 执行导出（实现数据库导出功能）
        bool success = exportDataToCSV(startDate, endDate, filePath, progressBar);

        if (success) {
            QMessageBox::information(this, "成功", "数据导出完成");
            exportDialog.accept();
        } else {
            QMessageBox::warning(this, "失败", "数据导出失败");
            exportButton->setEnabled(true);
        }
    });

    exportDialog.exec();
    */
}


// 更新树控件显示当前数据
void AutomaticWeather_1::updateTreeWithCurrentData()
{
    if (!m_isConnected) {
        return;
    }

    // 获取当前日期
    QDate currentDate = QDate::currentDate();
    QString dateStr = currentDate.toString("yyyy-MM-dd");

    // 更新当前日期的数据条数
    for (int i = 0; i < ui->tree->topLevelItemCount(); i++) {
        QTreeWidgetItem* item = ui->tree->topLevelItem(i);
        if (item->text(0) == dateStr) {
            // 这里不需要更新条数，因为数据保存时会更新
            // 只需要高亮显示
            QFont font = item->font(0);
            font.setBold(true);
            item->setFont(0, font);
            item->setForeground(0, QBrush(Qt::red));
            item->setForeground(1, QBrush(Qt::red));

            // 确保显示"采集中..."
            if (!item->text(1).contains("条")) {
                item->setText(1, "采集中...");
            }

            ui->tree->repaint();
            break;
        }
    }
}

// 添加日期到树控件
void AutomaticWeather_1::addDateToTree(const QDate& date)
{
    QString dateStr = date.toString("yyyy-MM-dd");

    // 创建树项
    QTreeWidgetItem* item = new QTreeWidgetItem(ui->tree);
    item->setText(0, dateStr);
    item->setText(1, "0条");  // 初始数据条数

    // 如果是今天的数据，设置为粗体
    if (date == QDate::currentDate()) {
        QFont font = item->font(0);
        font.setBold(true);
        item->setFont(0, font);
        item->setForeground(0, QBrush(Qt::red));
        item->setForeground(1, QBrush(Qt::red));
    }

    // 添加到树控件
    ui->tree->addTopLevelItem(item);

    // 记录这个日期
    m_treeDates.insert(dateStr);

    qDebug() << "添加日期到树控件:" << dateStr;
}

// 移除过旧的日期
void AutomaticWeather_1::removeOldDatesFromTree()
{
    QDate today = QDate::currentDate();
    QDate oldestDate = today.addDays(-30);  // 只保留最近30天的数据

    // 遍历树控件中的所有日期
    for (int i = ui->tree->topLevelItemCount() - 1; i >= 0; i--) {
        QTreeWidgetItem* item = ui->tree->topLevelItem(i);
        QString dateStr = item->text(0);
        QDate itemDate = QDate::fromString(dateStr, "yyyy-MM-dd");

        if (itemDate.isValid() && itemDate < oldestDate) {
            // 移除过旧的日期
            m_treeDates.remove(dateStr);
            delete ui->tree->takeTopLevelItem(i);
            qDebug() << "移除过旧日期:" << dateStr;
        }
    }
}


// 缓冲区数据准备好的处理函数（每60次调用一次）
void AutomaticWeather_1::onAveragedDataReady(const WeatherData& averagedData)
{
    qDebug() << "======================================";
    qDebug() << "数据缓冲区已满（60次数据），开始保存到数据库";
    qDebug() << "时间戳:" << averagedData.timestamp;
    qDebug() << "温度:" << averagedData.temperature << "°C";
    qDebug() << "湿度:" << averagedData.humidity << "%RH";
    qDebug() << "大气压:" << averagedData.pressure << "kPa";
    qDebug() << "露点温度:" << averagedData.dewPoint << "°C";
    qDebug() << "海拔:" << averagedData.altitude << "m";
    qDebug() << "含水量:" << averagedData.waterContent << "g/m³";
    qDebug() << "含水量百分比:" << averagedData.waterContentPercent << "%";
    qDebug() << "风速:" << averagedData.windSpeed << "m/s";
    qDebug() << "风向角度:" << averagedData.windDirectionAngle << "度";
    qDebug() << "风向描述:" << averagedData.windDirectionStr;

    // 保存到数据库
    DatabaseManager& dbManager = DatabaseManager::instance();
    if (dbManager.insertData(averagedData)) {
        qDebug() << "平均数据已成功保存到数据库";

        // 更新树控件中的数据显示条数
        updateDataCountInTree(averagedData.timestamp);
    } else {
        qDebug() << "平均数据保存到数据库失败";
    }
}

// 更新树控件中的数据条数显示
void AutomaticWeather_1::updateDataCountInTree(const QString& timestamp)
{
    // 从时间戳提取日期
    QDateTime dateTime = QDateTime::fromString(timestamp, "yyyy-MM-dd HH:mm:ss");
    QString dateStr = dateTime.toString("yyyy-MM-dd");

    // 查找树控件中的对应日期项
    bool found = false;
    for (int i = 0; i < ui->tree->topLevelItemCount(); i++) {
        QTreeWidgetItem* item = ui->tree->topLevelItem(i);
        if (item->text(0) == dateStr) {
            // 更新数据条数
            int currentCount = 0;
            QString currentText = item->text(1);

            // 从字符串中提取数字（支持"0条"、"采集中..."、"历史数据"等格式）
            if (currentText.contains("条")) {
                QString countStr = currentText;
                countStr = countStr.replace("条", "").trimmed();
                currentCount = countStr.toInt();
            } else if (currentText == "采集中..." || currentText == "历史数据") {
                currentCount = 1;  // 重新开始计数
            } else if (!currentText.isEmpty()) {
                // 尝试直接转换
                currentCount = currentText.toInt();
            }

            currentCount++;
            item->setText(1, QString::number(currentCount) + "条");

            // 强制刷新该项
            ui->tree->repaint();

            found = true;
            qDebug() << "更新树控件数据条数:" << dateStr << "->" << currentCount << "条";
            break;
        }
    }

    // 如果没有找到对应日期项，添加新项
    if (!found) {
        addDateToTree(dateTime.date());
        // 更新新添加的项
        updateDataCountInTree(timestamp);  // 递归调用一次
    }
}

// 添加头文件
#include <QCloseEvent>

// 添加关闭事件处理函数
void AutomaticWeather_1::closeEvent(QCloseEvent *event)
{
    qDebug() << "========== 应用程序关闭事件 ==========";
    qDebug() << "开始安全关闭程序...";

    // 如果正在采集，先停止
    if (m_isConnected) {
        qDebug() << "正在停止数据采集...";
        on_open_clicked();  // 调用关闭按钮逻辑

        // 等待一小段时间让设备完全断开
        QEventLoop loop;
        QTimer::singleShot(300, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // 确保所有定时器都停止
    if (m_treeUpdateTimer && m_treeUpdateTimer->isActive()) {
        m_treeUpdateTimer->stop();
    }

    // 确保数据缓冲区停止
    if (m_dataBuffer && m_dataBuffer->isRunning()) {
        m_dataBuffer->stop();
    }

    qDebug() << "程序可以安全关闭";
    event->accept();
}


//数据库初始化
void AutomaticWeather_1::initDatabase()
{
    if (!DatabaseManager::instance().initialize()) {
        QMessageBox::warning(this, "错误", "数据库初始化失败！");
    }
}


/**
 * 折线图控件
 */

void AutomaticWeather_1::on_temp_clicked()
{
    switchDataType(0);
}


void AutomaticWeather_1::on_RH_2_clicked()
{
    switchDataType(1);
}


void AutomaticWeather_1::on_pressure_clicked()
{
    switchDataType(2);
}


void AutomaticWeather_1::on_water_clicked()
{
    switchDataType(3);
}


void AutomaticWeather_1::on_water_percent_clicked()
{
    switchDataType(4);
}

void AutomaticWeather_1::on_dewPoint_clicked()
{
    switchDataType(5);
}



//数据类型选择
void AutomaticWeather_1::switchDataType(int dataType)
{
    if (m_currentDataType == dataType) return;
    m_currentDataType = dataType;

    auto setButtonStyle = [](QPushButton* btn, bool isSelected) {
        if (isSelected) {
            btn->setStyleSheet(
                "QPushButton { background-color: #1976D2; color: white; font-weight: bold; }"
                );
        } else {
            btn->setStyleSheet(
                "QPushButton { background-color: white; color: black; }"
                );
        }
    };

    // 根据 dataType 设置各个按钮的样式
    setButtonStyle(ui->temp,            dataType == 0);
    setButtonStyle(ui->RH_2,            dataType == 1);
    setButtonStyle(ui->pressure,        dataType == 2);
    setButtonStyle(ui->water,           dataType == 3);
    setButtonStyle(ui->water_percent,   dataType == 4);
    setButtonStyle(ui->dewPoint,        dataType == 5);

    updateChartDisplay();
}

void AutomaticWeather_1::refreshChart()
{
    updateChartDisplay();
}


void AutomaticWeather_1::updateChartDisplay()
{
    if (!ui->chartWidget) return;
    auto data = DatabaseManager::instance().queryDailyExtremes(MAX_DISPLAY_DAYS, m_currentDataType);
    ui->chartWidget->updateChart(data, m_currentDataType, MAX_DISPLAY_DAYS);
}




