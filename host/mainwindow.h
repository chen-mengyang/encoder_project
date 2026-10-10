#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QElapsedTimer>
#include <QSet>
#include <QSerialPort>
#include <QTimer>

#include "serialprotocol.h"

class QLabel;
class QListWidgetItem;
class AngleGauge;

namespace QtCharts {
class QChartView;
class QLineSeries;
class QValueAxis;
}

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onBtnRefreshPorts();
    void onBtnOpenClose();
    void onBtnEnumerate();
    void onBtnReadAngle();
    void onBtnReadVolt();
    void onBtnReadStatus();
    void onBtnSetAddr();
    void onPollToggled(bool on);
    void onPollIntervalChanged(int ms);
    void onPollTimeoutCheck();
    void onPollStep();
    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError err);

private:
    void setupCharts();
    void appendLog(const QString &text);
    void setLamp(QLabel *lamp, bool abnormal);
    void refreshDeviceList();
    quint8 currentTarget() const;
    bool sendCommand(quint8 func, const QByteArray &data = QByteArray(),
                     quint8 addr = 0xFF, bool showLog = true);
    void handleFrame(const ResponseFrame &rf);

    Ui::MainWindow *ui;
    QSerialPort *m_port = nullptr;

    AngleGauge *m_gauge = nullptr;
    QtCharts::QChartView *m_chartView = nullptr;
    QtCharts::QLineSeries *m_angleSeries = nullptr;
    QtCharts::QValueAxis *m_axisX = nullptr;
    QtCharts::QValueAxis *m_axisY = nullptr;

    QTimer m_timeoutTimer;      // 应答超时检查
    QTimer m_pollTimer;         // 自动轮询
    QElapsedTimer m_sendClock;

    bool m_waiting = false;     // 是否有未应答命令
    quint8 m_waitFunc = 0;
    bool m_enumerating = false;

    ResponseParser m_parser;
    QSet<quint8> m_devices;     // 已发现的从机地址集合
    int m_pollIndex = 0;

    double m_lastAngle = -1.0;  // 曲线展开用
    double m_unwrappedAngle = 0.0;
    int m_sampleCount = 0;
};

#endif // MAINWINDOW_H

