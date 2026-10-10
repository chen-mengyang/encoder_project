QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets serialport charts

TARGET = encoder_config
TEMPLATE = app

CONFIG += c++11

# 可执行文件输出到 build/ 目录
DESTDIR = build

# 构建中间产物输出到显式子目录，保持源码根目录整洁
OBJECTS_DIR = obj
MOC_DIR = moc
UI_DIR = ui
RCC_DIR = rcc

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    serialprotocol.cpp \
    gaugewidget.cpp

HEADERS += \
    mainwindow.h \
    serialprotocol.h \
    gaugewidget.h

FORMS += \
    mainwindow.ui

TRANSLATIONS += \
    encoder_config_zh_CN.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
