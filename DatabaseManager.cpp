// DatabaseManager.cpp
#include "DatabaseManager.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>

//  Qt 版本兼容处理
#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#elif QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
#include <QStringConverter>
#endif

DatabaseManager* DatabaseManager::m_instance = nullptr;

DatabaseManager& DatabaseManager::instance()
{
    if (!m_instance) {
        m_instance = new DatabaseManager();
    }
    return *m_instance;
}

DatabaseManager::DatabaseManager(QObject* parent)
    : QObject(parent)
{
    m_databasePath = QDir::currentPath() + "/weather_data.db";
}

DatabaseManager::~DatabaseManager()
{
    if (m_database.isOpen()) {
        m_database.close();
    }
}

bool DatabaseManager::initialize()
{
    QMutexLocker locker(&m_mutex);

    qDebug() << "======================================";
    qDebug() << "初始化数据库...";

    // 如果已经打开，先关闭
    if (m_database.isOpen()) {
        m_database.close();
        qDebug() << "关闭已存在的数据库连接";
    }

    // 设置数据库文件路径
    QString appPath = QDir::currentPath();
    m_databasePath = appPath + "/weather_data.db";
    qDebug() << "数据库文件路径:" << m_databasePath;

    // 检查目录是否存在
    QDir dir(appPath);
    if (!dir.exists()) {
        qDebug() << "应用程序目录不存在:" << appPath;
        return false;
    }

    // 检查文件是否可写
    QFileInfo fileInfo(m_databasePath);
    if (fileInfo.exists()) {
        qDebug() << "数据库文件已存在，大小:" << fileInfo.size() << "字节";
    } else {
        qDebug() << "数据库文件不存在，将创建新文件";
    }

    // 打开数据库
    m_database = QSqlDatabase::addDatabase("QSQLITE", "weather_connection");
    m_database.setDatabaseName(m_databasePath);

    if (!m_database.open()) {
        QString error = m_database.lastError().text();
        qDebug() << "无法打开数据库:" << error;
        qDebug() << "请检查：";
        qDebug() << "  1. 程序是否有写入权限";
        qDebug() << "  2. 文件是否被其他程序占用";
        qDebug() << "  3. 磁盘空间是否充足";
        return false;
    }

    qDebug() << "数据库连接成功";

    // 创建表格
    bool tablesCreated = createTables();
    if (tablesCreated) {
        qDebug() << "数据库表格创建成功";
    } else {
        qDebug() << "数据库表格创建失败";
        m_database.close();
        return false;
    }

    qDebug() << "数据库初始化完成";
    qDebug() << "======================================";

    return true;
}

bool DatabaseManager::createTables()
{
    QSqlQuery query(m_database);

    // 创建主表
    QString createTable = R"(
        CREATE TABLE IF NOT EXISTS weather_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            temperature REAL,
            humidity REAL,
            pressure REAL,
            dew_point REAL,
            altitude REAL,
            water_content REAL,
            water_content_percent REAL,
            wind_speed REAL,
            wind_direction_angle INTEGER,
            wind_direction_str TEXT,
            UNIQUE(timestamp)
        )
    )";

    if (!query.exec(createTable)) {
        qDebug() << "创建表失败:" << query.lastError().text();
        return false;
    }

    // 创建索引以提高查询性能
    query.exec("CREATE INDEX IF NOT EXISTS idx_timestamp ON weather_data(timestamp)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_date ON weather_data(date(timestamp))");

    return true;
}

bool DatabaseManager::insertData(const WeatherData& data)
{
    QMutexLocker locker(&m_mutex);

    // 检查数据库是否打开
    if (!m_database.isOpen()) {
        qDebug() << "数据库未打开，尝试重新初始化...";
        if (!initialize()) {
            qDebug() << "数据库重新初始化失败，无法插入数据";
            return false;
        }
    }

    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT OR REPLACE INTO weather_data
        (timestamp,
         temperature,
         humidity,
         pressure,
         dew_point, altitude,
         water_content,
         water_content_percent,
         wind_speed,
         wind_direction_angle,
         wind_direction_str)VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    query.addBindValue(data.timestamp);
    query.addBindValue(data.temperature);
    query.addBindValue(data.humidity);
    query.addBindValue(data.pressure);
    query.addBindValue(data.dewPoint);
    query.addBindValue(data.altitude);
    query.addBindValue(data.waterContent);
    query.addBindValue(data.waterContentPercent);
    query.addBindValue(data.windSpeed);
    query.addBindValue(data.windDirectionAngle);
    query.addBindValue(data.windDirectionStr);

    if (!query.exec()) {
        QString error = query.lastError().text();
        qDebug() << "插入数据失败:" << error;
        qDebug() << "SQL:" << query.lastQuery();
        qDebug() << "数据:" << data.timestamp << data.temperature << data.humidity;

        // 尝试重新打开数据库
        if (error.contains("no such table") || error.contains("database is locked")) {
            qDebug() << "数据库错误，尝试重新连接...";
            m_database.close();
            if (initialize()) {
                // 重新尝试插入
                return insertData(data);
            }
        }
        return false;
    }

    // 记录插入成功
    static int insertCount = 0;
    insertCount++;
    if (insertCount % 10 == 0) {
        qDebug() << "已成功插入" << insertCount << "条数据到数据库";
    }

    emit dataInserted();
    return true;
}

QVector<WeatherData> DatabaseManager::getDataByDate(const QDate& date)
{
    return getDataByDateRange(date, date);
}

QVector<WeatherData> DatabaseManager::getDataByDateRange(const QDate& start, const QDate& end)
{
    QMutexLocker locker(&m_mutex);
    QVector<WeatherData> result;

    if (!m_database.isOpen()) {
        if (!initialize()) {
            return result;
        }
    }

    QSqlQuery query(m_database);
    query.prepare(R"(
        SELECT timestamp, temperature, humidity, pressure, dew_point, altitude,
               water_content, water_content_percent, wind_speed,
               wind_direction_angle, wind_direction_str
        FROM weather_data
        WHERE date(timestamp) BETWEEN ? AND ?
        ORDER BY timestamp ASC
    )");

    query.addBindValue(start.toString("yyyy-MM-dd"));
    query.addBindValue(end.toString("yyyy-MM-dd"));

    if (query.exec()) {
        while (query.next()) {
            WeatherData data;
            data.timestamp = query.value(0).toString();
            data.temperature = query.value(1).toFloat();
            data.humidity = query.value(2).toFloat();
            data.pressure = query.value(3).toFloat();
            data.dewPoint = query.value(4).toFloat();
            data.altitude = query.value(5).toFloat();
            data.waterContent = query.value(6).toFloat();
            data.waterContentPercent = query.value(7).toFloat();
            data.windSpeed = query.value(8).toFloat();
            data.windDirectionAngle = query.value(9).toInt();
            data.windDirectionStr = query.value(10).toString();

            result.append(data);
        }
    } else {
        qDebug() << "查询数据失败:" << query.lastError().text();
    }

    return result;
}

QVector<QDate> DatabaseManager::getAvailableDates()
{
    QMutexLocker locker(&m_mutex);
    QVector<QDate> dates;

    if (!m_database.isOpen()) {
        if (!initialize()) {
            return dates;
        }
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT DISTINCT date(timestamp) FROM weather_data ORDER BY date(timestamp) DESC");

    if (query.exec()) {
        while (query.next()) {
            QDate date = QDate::fromString(query.value(0).toString(), "yyyy-MM-dd");
            if (date.isValid()) {
                dates.append(date);
            }
        }
    }

    return dates;
}

bool DatabaseManager::exportToCSV(const QString& filePath, const QDate& start, const QDate& end)
{
    QVector<WeatherData> data = getDataByDateRange(start, end);

    if (data.isEmpty()) {
        emit exportFinished(false, "指定日期范围内没有数据");
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit exportFinished(false, "无法创建文件");
        return false;
    }

    QTextStream stream(&file);
// 使用条件编译处理不同Qt版本
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt6 版本
    stream.setEncoding(QStringConverter::Utf8);
#elif QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    // Qt5.15+ 版本
    stream.setEncoding(QStringConverter::Utf8);
#else
    // Qt5.15 之前版本
    stream.setCodec("UTF-8");
#endif


    // 写入CSV头部
    stream << "时间戳,温度(℃),湿度(%RH),大气压(kPa),露点温度(℃),海拔(m),"
           << "含水量(g/m³),含水量百分比(%),风速(m/s),风向角度,风向描述\n";

    int total = data.size();
    for (int i = 0; i < total; i++) {
        const WeatherData& record = data[i];
        stream << record.timestamp << ","
               << record.temperature << ","
               << record.humidity << ","
               << record.pressure << ","
               << record.dewPoint << ","
               << record.altitude << ","
               << record.waterContent << ","
               << record.waterContentPercent << ","
               << record.windSpeed << ","
               << record.windDirectionAngle << ","
               << record.windDirectionStr << "\n";

        // 更新进度
        int percentage = (i + 1) * 100 / total;
        emit exportProgress(percentage);
    }

    file.close();
    emit exportFinished(true, QString("成功导出%1条数据").arg(total));
    return true;
}


QVector<QVector<QPointF>> DatabaseManager::queryDailyExtremes(int days, int dataType)
{
    QMutexLocker locker(&m_mutex);
    QVector<QVector<QPointF>> result;

    if(!m_database.isOpen())
    {
        if(!initialize())
        {
            return result;
        }
    }
    QString fieldName;
    switch (dataType) {
    case 0:
        fieldName = "temperature";            //温度
        break;
    case 1:
        fieldName = "humidity";               //湿度
        break;
    case 2:
        fieldName = "pressure";               //大气压
        break;
    case 3:
        fieldName = "water_content";          //含水量
        break;
    case 4:
        fieldName = "water_content_percent";  //含水量百分比
        break;
    case 5:
        fieldName = "dew_point";              //露点温度
        break;
    default:
        qDebug() << "未知数据类型" << dataType;
        return result;
    }


    QDateTime endTime = QDateTime::currentDateTime();
    QDateTime startTime = endTime.addDays(-days);

    QSqlQuery query(m_database);
    QString sql = QString(
                      "SELECT DATE(timestamp) as date, "
                      "MIN(%1) as min_value, MAX(%1) as max_value "
                      "FROM weather_data "
                      "WHERE timestamp >= :start_time AND %1 IS NOT NULL "
                      "GROUP BY DATE(timestamp) "
                      "ORDER BY date DESC "
                      "LIMIT :limit"
                      ).arg(fieldName);

    query.prepare(sql);
    query.bindValue(":start_time", startTime.toString("yyyy-MM-dd HH:mm:ss"));
    query.bindValue(":limit", days);

    if (!query.exec()) {
        qDebug() << "查询每日极值失败:" << query.lastError().text();
        return result;
    }

    int dayCount = 0;
    while(query.next() && dayCount < days)
    {
        QString dateStr = query.value("date").toString();
        double minValue = query.value("min_value").toDouble();
        double maxValue = query.value("max_value").toDouble();

        QDate date = QDate::fromString(dateStr, "yyyy-MM-dd");
        QDateTime dateTime(date, QTime(0, 0, 0));

        QVector<QPointF> dayPoints;
        dayPoints.append(QPointF(dateTime.toMSecsSinceEpoch(), minValue));
        dayPoints.append(QPointF(dateTime.toMSecsSinceEpoch(), maxValue));

        result.append(dayPoints);
        dayCount++;
    }
    return result;

}
