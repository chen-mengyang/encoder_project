#ifndef SERIALPROTOCOL_H
#define SERIALPROTOCOL_H

#include <QByteArray>
#include <QList>
#include <QString>

// ==================== 通讯协议常量（见 doc/protocol.md） ====================
namespace Protocol {

constexpr quint8 FRAME_HEAD0      = 0xAA;   // 帧头 0
constexpr quint8 FRAME_HEAD1      = 0xBB;   // 帧头 1
constexpr quint8 MAX_DATA_LEN     = 60;     // 数据区最大长度

// 功能码（主机 -> 从机）
constexpr quint8 FUNC_READ_ANGLE  = 0x01;   // 读 MT6816 角度
constexpr quint8 FUNC_READ_VOLT   = 0x02;   // 读 ADC 电压(mV)
constexpr quint8 FUNC_READ_STATUS = 0x03;   // 读状态字
constexpr quint8 FUNC_SET_ADDR    = 0x04;   // 设置从机地址（广播）
constexpr quint8 FUNC_GET_ADDR    = 0x05;   // 获取从机地址（广播）

// 应答码（从机 -> 主机）
constexpr quint8 RSP_READ_ANGLE   = 0x81;
constexpr quint8 RSP_READ_VOLT    = 0x82;
constexpr quint8 RSP_READ_STATUS  = 0x83;
constexpr quint8 RSP_SET_ADDR     = 0x84;
constexpr quint8 RSP_GET_ADDR     = 0x85;

// 地址
constexpr quint8 ADDR_BROADCAST   = 0x00;   // 广播地址
constexpr quint8 ADDR_MIN         = 0x01;
constexpr quint8 ADDR_MAX         = 0xFF;

// 状态字位定义
constexpr quint8 STATUS_ADDR_UNSET = 0x01;  // 地址未配置
constexpr quint8 STATUS_MAG_ERR    = 0x02;  // MT6816 无磁场/校验错误
constexpr quint8 STATUS_EEPROM_ERR = 0x04;  // EEPROM 通信失败

// MT6816 编码器
constexpr int ANGLE_MAX = 16383;            // 14 位角度最大值
}

// ==================== 应答帧解析结果 ====================
struct ResponseFrame
{
    bool valid = false;       // 帧头/校验和是否正确
    quint8 addr = 0;          // 应答地址
    quint8 rsp = 0;           // 应答码
    int len = 0;              // 数据长度
    QByteArray data;          // 数据区
    quint8 sum = 0;           // 帧内校验和
    QByteArray rawFrame;      // 原始帧（含帧头）
    QString error;            // 校验错误信息

    // 角度应答（RSP_READ_ANGLE）：14 位原始值 0~16383
    int angleRaw() const;
    // 换算角度 0~360°
    double angleDeg() const;
    // MT6816 状态 bit1 = No_Mag 磁场异常
    bool noMag() const;

    // 电压应答（RSP_READ_VOLT）：mV，大端
    int voltageMv() const;

    // 状态应答（RSP_READ_STATUS）：状态字
    quint8 statusWord() const;
};

// ==================== 协议工具 ====================
class SerialProtocol
{
public:
    // 构造请求帧：AA BB ADDR FUNC LEN DATA... SUM
    static QByteArray buildRequest(quint8 addr, quint8 func,
                                   const QByteArray &data = QByteArray());
    // 校验和 = (ADDR + FUNC/RSP + LEN + ΣDATA) & 0xFF
    static quint8 checksum(const QByteArray &body);
};

// ==================== 应答流解析器 ====================
// 累积字节流，按帧头同步并校验，提取完整应答帧
class ResponseParser
{
public:
    QList<ResponseFrame> push(const QByteArray &bytes);
    void reset() { m_buf.clear(); }

private:
    QByteArray m_buf;
};

#endif // SERIALPROTOCOL_H
