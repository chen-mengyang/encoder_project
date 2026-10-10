#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "gaugewidget.h"

#include <QCheckBox>
#include <QLabel>
#include <QListWidgetItem>
#include <QPen>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSerialPortInfo>
#include <QSpinBox>
#include <QTime>
#include <QVBoxLayout>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    m_port = new QSerialPort(this);
    connect(m_port, &QSerialPort::readyRead, this, &MainWindow::onReadyRead);
    connect(m_port, &QSerialPort::errorOccurred, this, &MainWindow::onSerialError);

    connect(ui->btnRefreshPorts, &QPushButton::clicked, this, &MainWindow::onBtnRefreshPorts);
    connect(ui->btnOpenClose,     &QPushButton::clicked, this, &MainWindow::onBtnOpenClose);
    connect(ui->btnEnumerate,     &QPushButton::clicked, this, &MainWindow::onBtnEnumerate);
    connect(ui->btnReadAngle,     &QPushButton::clicked, this, &MainWindow::onBtnReadAngle);
    connect(ui->btnReadVolt,      &QPushButton::clicked, this, &MainWindow::onBtnReadVolt);
    connect(ui->btnReadStatus,    &QPushButton::clicked, this, &MainWindow::onBtnReadStatus);
    connect(ui->btnSetAddr,       &QPushButton::clicked, this, &MainWindow::onBtnSetAddr);
    connect(ui->checkAutoPoll,    &QCheckBox::toggled, this, &MainWindow::onPollToggled);
    connect(ui->spinPollInterval, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onPollIntervalChanged);

    m_timeoutTimer.setInterval(50);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &MainWindow::onPollTimeoutCheck);
    m_timeoutTimer.start();

    m_pollTimer.setInterval(ui->spinPollInterval->value());
    connect(&m_pollTimer, &QTimer::timeout, this, &MainWindow::onPollStep);

    m_gauge = ui->gaugeWidget;
    setupCharts();

    setLamp(ui->lampAddrUnset, false);
    setLamp(ui->lampMagErr, false);
    setLamp(ui->lampEepromErr, false);

    // 端口框可编辑：允许手动输入 QSerialPortInfo 未枚举到的端口（如 pty / 虚拟串口）
    ui->comboPort->setEditable(true);
    // 协议默认波特率 115200
    ui->comboBaud->setCurrentText(QStringLiteral("115200"));

    onBtnRefreshPorts();
    refreshDeviceList();
    appendLog(QStringLiteral("[信息] 编码器配置工具已启动（MT6816 / 半双工 UART 单总线）"));
}

MainWindow::~MainWindow()
{
    if (m_port && m_port->isOpen())
        m_port->close();
    delete ui;
}

void MainWindow::setupCharts()
{
    m_angleSeries = new QtCharts::QLineSeries();
    m_angleSeries->setName(QStringLiteral("角度"));
    m_angleSeries->setPen(QPen(QColor(0x34, 0x98, 0xdb), 2));

    m_axisX = new QtCharts::QValueAxis();
    m_axisX->setRange(0, 300);
    m_axisX->setTitleText(QStringLiteral("采样点"));
    m_axisX->setLabelFormat("%d");

    m_axisY = new QtCharts::QValueAxis();
    m_axisY->setRange(0, 360);
    m_axisY->setTitleText(QStringLiteral("角度(°)"));
    m_axisY->setLabelFormat("%.0f");

    QtCharts::QChart *chart = new QtCharts::QChart();
    chart->addSeries(m_angleSeries);
    chart->addAxis(m_axisX, Qt::AlignBottom);
    chart->addAxis(m_axisY, Qt::AlignLeft);
    m_angleSeries->attachAxis(m_axisX);
    m_angleSeries->attachAxis(m_axisY);
    chart->legend()->hide();
    chart->setTitle(QStringLiteral("角度实时曲线（最近 300 点，已展开消除跳变）"));

    m_chartView = new QtCharts::QChartView(chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    auto *layout = new QVBoxLayout(ui->chartContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_chartView);
}

void MainWindow::appendLog(const QString &text)
{
    ui->logText->appendPlainText(QStringLiteral("[%1] %2")
                                     .arg(QTime::currentTime().toString("HH:mm:ss.zzz"), text));
    QScrollBar *sb = ui->logText->verticalScrollBar();
    if (sb)
        sb->setValue(sb->maximum());
}

void MainWindow::setLamp(QLabel *lamp, bool abnormal)
{
    if (!lamp)
        return;
    lamp->setFixedSize(16, 16);
    lamp->setStyleSheet(abnormal
        ? QStringLiteral("background-color:#e74c3c;border-radius:8px;border:1px solid #c0392b;")
        : QStringLiteral("background-color:#2ecc71;border-radius:8px;border:1px solid #27ae60;"));
}

void MainWindow::refreshDeviceList()
{
    ui->listDevices->clear();
    QList<quint8> addrs = m_devices.values();
    std::sort(addrs.begin(), addrs.end());
    for (quint8 a : addrs) {
        auto *item = new QListWidgetItem(QStringLiteral("从机 0x%1")
                                             .arg(a, 2, 16, QLatin1Char('0')).toUpper());
        item->setData(Qt::UserRole, a);
        ui->listDevices->addItem(item);
    }
    if (ui->listDevices->count() > 0)
        ui->listDevices->setCurrentRow(0);
}

quint8 MainWindow::currentTarget() const
{
    QListWidgetItem *item = ui->listDevices->currentItem();
    if (!item)
        return 0xFF;
    bool ok = false;
    const quint8 a = quint8(item->data(Qt::UserRole).toUInt(&ok));
    return ok ? a : 0xFF;
}

bool MainWindow::sendCommand(quint8 func, const QByteArray &data, quint8 addr, bool showLog)
{
    if (!m_port || !m_port->isOpen()) {
        appendLog(QStringLiteral("[错误] 串口未打开"));
        return false;
    }
    if (m_waiting) {
        appendLog(QStringLiteral("[提示] 上一条命令（FUNC=0x%1）尚未收到应答")
                      .arg(m_waitFunc, 2, 16, QLatin1Char('0')));
        return false;
    }
    if (addr == 0xFF) {
        addr = currentTarget();
        if (addr == 0xFF) {
            appendLog(QStringLiteral("[错误] 请先在“总线从机”列表中选择目标从机"));
            return false;
        }
    }
    m_parser.reset();
    const QByteArray frame = SerialProtocol::buildRequest(addr, func, data);
    const qint64 written = m_port->write(frame);
    m_port->flush();
    if (written != frame.size()) {
        appendLog(QStringLiteral("[错误] 串口写入失败：%1").arg(m_port->errorString()));
        return false;
    }
    m_waiting = true;
    m_waitFunc = func;
    m_sendClock.start();
    if (showLog) {
        appendLog(QStringLiteral("[发送] 地址=0x%1 FUNC=0x%2 LEN=%3 数据=[%4] 校验=0x%5")
                      .arg(addr, 2, 16, QLatin1Char('0'))
                      .arg(func, 2, 16, QLatin1Char('0'))
                      .arg(data.size())
                      .arg(data.isEmpty() ? QStringLiteral("-")
                                          : QString::fromLatin1(data.toHex(' ')).toUpper())
                      .arg(quint8(frame.at(frame.size() - 1)), 2, 16, QLatin1Char('0')));
        appendLog(QStringLiteral("       帧: %1")
                      .arg(QString::fromLatin1(frame.toHex(' ')).toUpper()));
    }
    return true;
}

void MainWindow::onReadyRead()
{
    if (!m_port)
        return;
    const QByteArray bytes = m_port->readAll();
    if (bytes.isEmpty())
        return;
    const QList<ResponseFrame> frames = m_parser.push(bytes);
    for (const ResponseFrame &rf : frames)
        handleFrame(rf);
}

void MainWindow::handleFrame(const ResponseFrame &rf)
{
    if (!rf.valid) {
        appendLog(QStringLiteral("[接收] 无效帧 RSP=0x%1 帧=[%2] %3")
                      .arg(rf.rsp, 2, 16, QLatin1Char('0'))
                      .arg(QString::fromLatin1(rf.rawFrame.toHex(' ')).toUpper())
                      .arg(rf.error));
        m_waiting = false;
        return;
    }

    // 半双工单线回环：从机收到请求时数据会回显到主机 RX，回显帧的 RSP 字段实为 FUNC(0x01~0x05)。
    // 协议应答码均为 0x81~0x85，故 RSP<0x80 的帧不是应答数据，静默丢弃（不影响等待状态）。
    if (rf.rsp < 0x80)
        return;

    appendLog(QStringLiteral("[接收] 地址=0x%1 RSP=0x%2 LEN=%3 数据=[%4] 校验=0x%5")
                  .arg(rf.addr, 2, 16, QLatin1Char('0'))
                  .arg(rf.rsp, 2, 16, QLatin1Char('0'))
                  .arg(rf.len)
                  .arg(rf.data.isEmpty() ? QStringLiteral("-")
                                         : QString::fromLatin1(rf.data.toHex(' ')).toUpper())
                  .arg(rf.sum, 2, 16, QLatin1Char('0')));

    switch (rf.rsp) {
    case Protocol::RSP_READ_ANGLE: {
        const int raw = rf.angleRaw();
        if (raw < 0)
            break;
        const double deg = rf.angleDeg();
        const bool noMag = rf.noMag();
        ui->lblAngleRaw->setText(QStringLiteral("原始值: %1 (0x%2)")
                                     .arg(raw)
                                     .arg(raw, 4, 16, QLatin1Char('0')).toUpper());
        ui->lblAngleDeg->setText(QStringLiteral("%1°").arg(deg, 0, 'f', 2));
        ui->lblMag->setText(noMag ? QStringLiteral("磁场: 异常(No_Mag)")
                                  : QStringLiteral("磁场: 正常"));
        m_gauge->setAngle(deg);

        if (m_lastAngle < 0)
            m_unwrappedAngle = deg;
        else {
            double d = deg - m_lastAngle;
            if (d > 180.0) d -= 360.0;
            else if (d < -180.0) d += 360.0;
            m_unwrappedAngle += d;
        }
        m_lastAngle = deg;
        m_angleSeries->append(++m_sampleCount, m_unwrappedAngle);
        const int win = 300;
        while (m_angleSeries->count() > win)
            m_angleSeries->remove(0);
        if (m_axisX && m_axisY) {
            const QVector<QPointF> pts = m_angleSeries->pointsVector();
            if (!pts.isEmpty()) {
                qreal xMin = pts.first().x(), xMax = pts.first().x();
                qreal yMin = pts.first().y(), yMax = pts.first().y();
                for (const QPointF &p : pts) {
                    xMin = qMin(xMin, p.x());
                    xMax = qMax(xMax, p.x());
                    yMin = qMin(yMin, p.y());
                    yMax = qMax(yMax, p.y());
                }
                m_axisX->setRange(qMax<qreal>(0, xMax - win), xMax + 1);
                qreal pad = qMax(qreal(1.0), (yMax - yMin) * 0.08);
                m_axisY->setRange(yMin - pad, yMax + pad);
            }
        }
        break;
    }
    case Protocol::RSP_READ_VOLT: {
        const int raw = rf.voltageMv();
        if (raw < 0)
            break;
        // 硬件分压电阻补偿：真实电压 = ADC 采样值 × 2（分压比 1:2，写死）
        const int mv = qRound(raw * 2.0f);
        ui->lblVoltV->setText(QStringLiteral("%1 V").arg(mv / 1000.0, 0, 'f', 3));
        ui->lblVoltMv->setText(QStringLiteral("%1 mV").arg(mv));
        break;
    }
    case Protocol::RSP_READ_STATUS: {
        const quint8 st = rf.statusWord();
        setLamp(ui->lampAddrUnset, st & Protocol::STATUS_ADDR_UNSET);
        setLamp(ui->lampMagErr,    st & Protocol::STATUS_MAG_ERR);
        setLamp(ui->lampEepromErr, st & Protocol::STATUS_EEPROM_ERR);
        break;
    }
    case Protocol::RSP_SET_ADDR: {
        if (!rf.data.isEmpty()) {
            const quint8 newAddr = quint8(rf.data.at(0));
            appendLog(QStringLiteral("       → 地址设置成功！新地址=0x%1（已写入 EEPROM）")
                          .arg(newAddr, 2, 16, QLatin1Char('0')));
            QTimer::singleShot(200, this, [this]() { onBtnEnumerate(); });
        }
        break;
    }
    case Protocol::RSP_GET_ADDR: {
        if (!rf.data.isEmpty()) {
            const quint8 a = quint8(rf.data.at(0));
            if (a != Protocol::ADDR_BROADCAST && !m_devices.contains(a)) {
                m_devices.insert(a);
                appendLog(QStringLiteral("       → 发现从机 0x%1")
                              .arg(a, 2, 16, QLatin1Char('0')));
                refreshDeviceList();
            }
        }
        break;
    }
    default:
        appendLog(QStringLiteral("       → 未知应答码 0x%1")
                      .arg(rf.rsp, 2, 16, QLatin1Char('0')));
        break;
    }

    if (rf.rsp == m_waitFunc + 0x80)
        m_waiting = false;
}

void MainWindow::onBtnRefreshPorts()
{
    const QString prev = ui->comboPort->currentText();
    ui->comboPort->clear();
    const QList<QSerialPortInfo> infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        QString desc = info.description().trimmed();
        if (desc.isEmpty())
            desc = info.manufacturer().trimmed();
        const QString label = desc.isEmpty()
            ? info.portName()
            : QStringLiteral("%1 (%2)").arg(info.portName(), desc);
        ui->comboPort->addItem(label, info.portName());
    }
    const int idx = ui->comboPort->findData(prev);
    if (idx >= 0)
        ui->comboPort->setCurrentIndex(idx);
    else
        ui->comboPort->setCurrentText(prev);      // 保留手动输入
    appendLog(QStringLiteral("[信息] 刷新串口列表：共 %1 个可用串口").arg(ui->comboPort->count()));
}

void MainWindow::onBtnOpenClose()
{
    if (m_port && m_port->isOpen()) {
        m_pollTimer.stop();
        ui->checkAutoPoll->setChecked(false);
        m_port->close();
        ui->btnOpenClose->setText(QStringLiteral("打开串口"));
        statusBar()->showMessage(QStringLiteral("串口已关闭"));
        appendLog(QStringLiteral("[信息] 串口已关闭"));
        return;
    }
    // 端口名解析：选中标准项时取该项 data；否则视为手动输入（如 pty / 虚拟串口）
    QString portName;
    const int ci = ui->comboPort->currentIndex();
    if (ci >= 0 && ui->comboPort->currentText() == ui->comboPort->itemText(ci))
        portName = ui->comboPort->itemData(ci).toString();
    if (portName.isEmpty())
        portName = ui->comboPort->currentText().section(' ', 0, 0).trimmed();
    if (portName.isEmpty()) {
        appendLog(QStringLiteral("[错误] 请先选择或输入串口"));
        return;
    }
    m_port->setPortName(portName);
    m_port->setBaudRate(ui->comboBaud->currentText().toInt());
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setFlowControl(QSerialPort::NoFlowControl);
    if (m_port->open(QIODevice::ReadWrite)) {
        ui->btnOpenClose->setText(QStringLiteral("关闭串口"));
        statusBar()->showMessage(QStringLiteral("已连接 %1 @ %2 bps")
                                     .arg(portName).arg(m_port->baudRate()));
        appendLog(QStringLiteral("[信息] 串口已打开：%1 @ %2 bps 8N1")
                      .arg(portName).arg(m_port->baudRate()));
        QTimer::singleShot(300, this, [this]() { onBtnEnumerate(); });
    } else {
        appendLog(QStringLiteral("[错误] 打开串口 %1 失败：%2").arg(portName, m_port->errorString()));
    }
}

void MainWindow::onBtnEnumerate()
{
    if (!m_port || !m_port->isOpen()) {
        appendLog(QStringLiteral("[错误] 串口未打开，无法枚举从机"));
        return;
    }
    m_devices.clear();
    refreshDeviceList();
    m_enumerating = true;
    appendLog(QStringLiteral("[枚举] 广播 CMD_GET_ADDR(0x05) ..."));
    sendCommand(Protocol::FUNC_GET_ADDR, QByteArray(), Protocol::ADDR_BROADCAST);
    QTimer::singleShot(400, this, [this]() {
        m_enumerating = false;
        appendLog(QStringLiteral("[枚举] 完成，共发现 %1 个从机").arg(m_devices.size()));
    });
}

void MainWindow::onBtnSetAddr()
{
    const quint8 newAddr = quint8(ui->spinNewAddr->value());
    if (newAddr == Protocol::ADDR_BROADCAST) {
        appendLog(QStringLiteral("[错误] 新地址不能为 0x00（广播地址）"));
        return;
    }
    appendLog(QStringLiteral("[设置地址] 广播 CMD_SET_ADDR(0x04) → 0x%1")
                  .arg(newAddr, 2, 16, QLatin1Char('0')));
    sendCommand(Protocol::FUNC_SET_ADDR, QByteArray(1, char(newAddr)),
                Protocol::ADDR_BROADCAST);
}

void MainWindow::onBtnReadAngle()
{
    sendCommand(Protocol::FUNC_READ_ANGLE);
}

void MainWindow::onBtnReadVolt()
{
    sendCommand(Protocol::FUNC_READ_VOLT);
}

void MainWindow::onBtnReadStatus()
{
    sendCommand(Protocol::FUNC_READ_STATUS);
}

void MainWindow::onPollToggled(bool on)
{
    if (on) {
        m_pollTimer.start();
        appendLog(QStringLiteral("[信息] 自动轮询已开启（间隔 %1 ms）").arg(m_pollTimer.interval()));
    } else {
        m_pollTimer.stop();
        appendLog(QStringLiteral("[信息] 自动轮询已关闭"));
    }
}

void MainWindow::onPollIntervalChanged(int ms)
{
    m_pollTimer.setInterval(ms);
    if (m_pollTimer.isActive())
        appendLog(QStringLiteral("[信息] 轮询间隔已调整为 %1 ms").arg(ms));
}

void MainWindow::onPollStep()
{
    if (!m_port || !m_port->isOpen())
        return;
    if (m_enumerating || m_waiting)
        return;
    static const quint8 funcs[3] = {
        Protocol::FUNC_READ_ANGLE,
        Protocol::FUNC_READ_VOLT,
        Protocol::FUNC_READ_STATUS
    };
    sendCommand(funcs[m_pollIndex % 3]);
    ++m_pollIndex;
}

void MainWindow::onPollTimeoutCheck()
{
    if (m_waiting && m_sendClock.isValid() && m_sendClock.elapsed() > 200) {
        m_waiting = false;
        appendLog(QStringLiteral("[超时] 未收到应答（FUNC=0x%1），请检查从机是否在线")
                      .arg(m_waitFunc, 2, 16, QLatin1Char('0')));
    }
}

void MainWindow::onSerialError(QSerialPort::SerialPortError err)
{
    if (err == QSerialPort::ResourceError || err == QSerialPort::DeviceNotFoundError) {
        if (m_port && m_port->isOpen()) {
            appendLog(QStringLiteral("[错误] 串口异常：%1").arg(m_port->errorString()));
            m_port->close();
            ui->btnOpenClose->setText(QStringLiteral("打开串口"));
            statusBar()->showMessage(QStringLiteral("串口异常关闭"));
        }
    }
}






