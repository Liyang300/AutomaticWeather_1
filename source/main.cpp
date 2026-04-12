#include "AutomaticWeather_1.h"

#include <QApplication>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);   //应用程序对象有且只能有一个
    AutomaticWeather_1 w;         //定义一个窗口对象
    w.show();                     //显示窗口


    return a.exec();              //应用程序事件循环
}
