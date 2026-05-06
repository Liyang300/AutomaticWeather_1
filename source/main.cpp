#if 0
#include "AutomaticWeather_1.h"
#include <QApplication>

#include "logger.h"
#include <clocale>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char *argv[])
{

    // 设置控制台编码为 UTF-8（解决 Windows 下中文乱码）
    std::setlocale(LC_ALL, ".UTF-8");
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    LOG_INFO("程序开始");
    QApplication a(argc, argv);   //应用程序对象有且只能有一个
    AutomaticWeather_1 w;         //定义一个窗口对象
    w.show();                     //显示窗口


    return a.exec();              //应用程序事件循环
}

#endif

#if 1

#include "AutomaticWeather_1.h"
#include "logger.h"          // 您的日志类（提供了 LOG_INFO 等宏）
#include <QApplication>
#include <QDebug>
#include <clocale>


// 自定义 Qt 消息处理器，将 qDebug 等重定向到您的日志系统
void qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
#ifdef _WIN32
    // 把当前进程的控制台输出强制设为 UTF-8
    SetConsoleOutputCP(65001);
    // 可选：同时把输入也设为 UTF-8
    SetConsoleCP(65001);
#endif

    QByteArray utf8Msg = msg.toUtf8();   // 统一转为 UTF-8
    const char* file = context.file ? context.file : "";
    int line = context.line;


    // logger.h 中有：
    // #define LOG_DEBUG(...)   NEXUS::Logger::instance().debug(__VA_ARGS__)
    // #define LOG_INFO(...)    NEXUS::Logger::instance().info(__VA_ARGS__)
    // #define LOG_WARN(...)    NEXUS::Logger::instance().warn(__VA_ARGS__)
    // #define LOG_ERROR(...)   NEXUS::Logger::instance().error(__VA_ARGS__)
    switch (type) {
    case QtDebugMsg:
        LOG_INFO("[QT] {} ({}:{})", utf8Msg.constData(), file, line);
        break;
    case QtInfoMsg:
        LOG_INFO("[QT] {} ({}:{})", utf8Msg.constData(), file, line);
        break;
    case QtWarningMsg:
        LOG_WARN("[QT] {} ({}:{})", utf8Msg.constData(), file, line);
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        LOG_ERROR("[QT] {} ({}:{})", utf8Msg.constData(), file, line);
        if (type == QtFatalMsg) abort();
        break;
    }
}

int main(int argc, char *argv[])
{
    //  设置 UTF-8 环境（解决中文乱码）
    std::setlocale(LC_ALL, ".UTF-8");
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    //  安装 Qt 消息处理器（必须在 QApplication 创建之后、任何 qDebug 之前）
    qInstallMessageHandler(qtMessageHandler);

    //  创建 QApplication
    QApplication a(argc, argv);

    //  触发日志系统初始化（单例自动构造，无需手动调用）
    //    这一行可选，因为后续 LOG_* 宏首次调用时也会初始化
    NEXUS::Logger::instance();

    //  显示主窗口
    AutomaticWeather_1 w;
    w.show();

    //  测试中文输出
    qDebug() << "应用程序启动，中文测试：天气数据加载中……";

    return a.exec();
}

#endif
