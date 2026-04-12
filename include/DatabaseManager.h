// DatabaseManager.h
#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QMutex>
#include <QVector>


#include <QPointF>

struct WeatherData {
    QString timestamp;
    float temperature;
    float humidity;
    float pressure;
    float dewPoint;
    float altitude;
    float waterContent;
    float waterContentPercent;
    float windSpeed;
    int windDirectionAngle;
    QString windDirectionStr;

    WeatherData()
        : temperature(0), humidity(0), pressure(0), dewPoint(0),
        altitude(0), waterContent(0), waterContentPercent(0),
        windSpeed(0), windDirectionAngle(0) {}
};

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    static DatabaseManager& instance();

    bool initialize();
    bool insertData(const WeatherData& data);
    QVector<WeatherData> getDataByDate(const QDate& date);
    QVector<WeatherData> getDataByDateRange(const QDate& start, const QDate& end);
    QVector<QDate> getAvailableDates();
    bool exportToCSV(const QString& filePath, const QDate& start, const QDate& end);

    QString databasePath() const { return m_databasePath; }
    QVector<QVector<QPointF>> queryDailyExtremes(int days, int dataType);

signals:
    void dataInserted();
    void exportProgress(int percentage);
    void exportFinished(bool success, const QString& message);

private:
    DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager();

    static DatabaseManager* m_instance;
    QSqlDatabase            m_database;
    QString                 m_databasePath;
    QMutex                  m_mutex;

    bool createTables();
};

#endif // DATABASEMANAGER_H
