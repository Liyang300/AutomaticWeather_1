#请求最小cmake版本
cmake_minimum_required(VERSION 3.20)

project(AutomaticWeather_1 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#查找包/模块
#find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
#find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets
                                                  SerialPort
                                                  SerialBus    #modbus组件
                                                  Sql          # 添加SQL支持
                                                  Charts       # chart支持
                                                  Concurrent
                                              )



find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets
                                                       SerialPort
                                                       SerialBus    #modbus组件
                                                       Sql          # 添加SQL支持
                                                       Charts       # chart支持
                                                       Concurrent
                                                   )
#find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
#find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
    source/main.cpp
    source/AutomaticWeather_1.cpp
    source/AutomaticWeather_1.ui
    source/SerialPortManager.cpp
    source/ModbusManager.cpp
    source/SensorDataParser.cpp
    source/CompassWidget.cpp
    source/DatabaseManager.cpp
    source/DataBuffer.cpp
    source/ExportDialog.cpp
    source/CustomChartWidget.cpp
    source/logger.cpp

    include/AutomaticWeather_1.h
    include/SerialPortManager.h
    include/ModbusManager.h
    include/SensorDataParser.h
    include/CompassWidget.h
    include/DatabaseManager.h
    include/DataBuffer.h
    include/ExportDialog.h
    include/CustomChartWidget.h
    include/logger.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(AutomaticWeather_1
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )

# Define target properties for Android with Qt 6 as:
#    set_property(TARGET AutomaticWeather_1 APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(AutomaticWeather_1 SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(AutomaticWeather_1
            ${PROJECT_SOURCES}
        )
    endif()
endif()

# 包含目录
#target_include_directories(AutomaticWeather_1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(AutomaticWeather_1 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

#把库添加到目标
target_link_libraries(AutomaticWeather_1 PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
    Qt${QT_VERSION_MAJOR}::SerialBus    # 添加modbus模块
    Qt${QT_VERSION_MAJOR}::Sql          # 添加SQL支持
    Qt${QT_VERSION_MAJOR}::Charts       # chart支持
    Qt${QT_VERSION_MAJOR}::Concurrent

)
target_link_libraries(AutomaticWeather_1 PRIVATE Qt${QT_VERSION_MAJOR}::Core)
target_link_libraries(AutomaticWeather_1 PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)



# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.AutomaticWeather_1)
endif()
set_target_properties(AutomaticWeather_1 PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS AutomaticWeather_1
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(AutomaticWeather_1)
endif()
