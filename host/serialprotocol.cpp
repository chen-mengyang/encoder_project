#include "serialprotocol.h"

int ResponseFrame::angleRaw() const
{
    if (!valid || rsp != Protocol::RSP_READ_ANGLE || data.size() < 2)
        return -1;
    const int h = quint8(data.at(0)) & 0x3F;   // 高 6 位（bit5~bit0）
    const int l = quint8(data.at(1));          // 低 8 位
    return (h << 8) | l;
}

double ResponseFrame::angleDeg() const
{
    const int raw = angleRaw();
    if (raw < 0)
        return -1.0;
    return raw * 360.0 / Protocol::ANGLE_MAX;
}

bool ResponseFrame::noMag() const
{
    if (!valid || rsp != Protocol::RSP_READ_ANGLE || data.size() < 3)
        return false;
    return (quint8(data.at(2)) & 0x02) != 0;   // MT6816 bit1 = No_Mag 警告
}

int ResponseFrame::voltageMv() const
{
    if (!valid || rsp != Protocol::RSP_READ_VOLT || data.size() < 2)
        return -1;
    return (quint8(data.at(0)) << 8) | quint8(data.at(1));   // 大端
}

quint8 ResponseFrame::statusWord() const
{
    if (!valid || rsp != Protocol::RSP_READ_STATUS || data.isEmpty())
        return 0;
    return quint8(data.at(0));
}

quint8 SerialProtocol::checksum(const QByteArray &body)
{
    quint8 sum = 0;
    for (int i = 0; i < body.size(); ++i)
        sum = quint8(sum + quint8(body.at(i)));
    return sum;
}

QByteArray SerialProtocol::buildRequest(quint8 addr, quint8 func, const QByteArray &data)
{
    QByteArray body;
    body.append(char(addr));
    body.append(char(func));
    body.append(char(data.size()));
    body.append(data);
    QByteArray frame;
    frame.append(char(Protocol::FRAME_HEAD0));
    frame.append(char(Protocol::FRAME_HEAD1));
    frame.append(body);
    frame.append(char(checksum(body)));
    return frame;
}

QList<ResponseFrame> ResponseParser::push(const QByteArray &bytes)
{
    m_buf.append(bytes);
    QList<ResponseFrame> frames;
    while (m_buf.size() >= 6) {
        // 查找帧头 AA BB
        int h = -1;
        for (int i = 0; i + 1 < m_buf.size(); ++i) {
            if (quint8(m_buf.at(i)) == Protocol::FRAME_HEAD0 &&
                quint8(m_buf.at(i + 1)) == Protocol::FRAME_HEAD1) {
                h = i;
                break;
            }
        }
        if (h < 0) {              // 无帧头，丢弃全部缓冲
            m_buf.clear();
            break;
        }
        if (h > 0) {              // 丢弃帧头前的杂数据
            m_buf.remove(0, h);
            continue;
        }
        // h == 0：开始解析
        const int len = quint8(m_buf.at(4));
        if (len > Protocol::MAX_DATA_LEN) {   // 长度非法，重新同步
            m_buf.remove(0, 2);
            continue;
        }
        const int total = 6 + len;
        if (m_buf.size() < total)
            break;                            // 数据未完整，等待后续字节
        const QByteArray frame = m_buf.left(total);
        m_buf.remove(0, total);

        ResponseFrame rf;
        rf.addr = quint8(frame.at(2));
        rf.rsp = quint8(frame.at(3));
        rf.len = len;
        rf.data = frame.mid(5, len);
        rf.sum = quint8(frame.at(total - 1));
        rf.rawFrame = frame;

        const quint8 calc = SerialProtocol::checksum(frame.mid(2, len + 3));
        if (calc == rf.sum) {
            rf.valid = true;
        } else {
            rf.error = QStringLiteral("校验和错误（计算 0x%1 / 帧内 0x%2）")
                           .arg(calc, 2, 16, QLatin1Char('0'))
                           .arg(rf.sum, 2, 16, QLatin1Char('0'));
        }
        frames.append(rf);
    }
    return frames;
}
