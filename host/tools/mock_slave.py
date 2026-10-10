#!/usr/bin/env python3
"""
MT6816 编码器从机模拟器（上位机调试工具）

通过 pty 创建一对虚拟串口：
  - 一端作为真实串口设备（打印 MOCK_PORT=/dev/pts/N），供上位机程序打开；
  - 另一端由本脚本模拟 STM32G030 从机，按 doc/protocol.md 协议应答。

用法：
    python3 mock_slave.py [--addr 0x01] [--angle 5000] [--voltage 3300] \
                          [--status 0] [--debug]

    --addr    模拟从机地址（默认 0x01）
    --angle   读角度命令返回的 14 位原始值（0~16383）
    --voltage 读电压命令返回的电压，单位 mV
    --status  读状态命令返回的状态字（bit0 地址未配置 / bit1 磁场错误 / bit2 EEPROM 错误）
    --debug   打印收发帧 hex

示例：
    python3 mock_slave.py --addr 0x01 --angle 12345 --voltage 3300 --debug
"""

import argparse
import os
import pty
import select
import sys
import termios
import tty

FRAME_HEAD = bytes([0xAA, 0xBB])


def checksum(body):
    return sum(body) & 0xFF


def build_response(addr, rsp, data):
    body = bytes([addr, rsp, len(data)]) + bytes(data)
    return FRAME_HEAD + body + bytes([checksum(body)])


class Slave:
    def __init__(self, addr, angle, voltage, status):
        self.addr = addr
        self.angle = angle
        self.voltage = voltage
        self.status = status

    def handle(self, frame):
        if len(frame) < 6:
            return None
        addr, func, ln = frame[2], frame[3], frame[4]
        if 6 + ln > len(frame):
            return None
        data = frame[5:5 + ln]
        body = frame[2:5 + ln]
        if checksum(body) != frame[5 + ln]:
            return None  # 校验失败，丢弃

        if func == 0x01:  # CMD_READ_ANGLE -> RSP_READ_ANGLE(0x81) 4B
            raw = int(self.angle) & 0x3FFF
            return build_response(self.addr, 0x81, [(raw >> 8) & 0x3F, raw & 0xFF, 0x00, 0x00])
        if func == 0x02:  # CMD_READ_VOLT -> RSP_READ_VOLT(0x82) 2B 大端
            mv = int(self.voltage) & 0xFFFF
            return build_response(self.addr, 0x82, [(mv >> 8) & 0xFF, mv & 0xFF])
        if func == 0x03:  # CMD_READ_STATUS -> RSP_READ_STATUS(0x83) 1B
            return build_response(self.addr, 0x83, [self.status & 0x07])
        if func == 0x04:  # CMD_SET_ADDR -> RSP_SET_ADDR(0x84) 1B
            if len(data) >= 1 and data[0] != 0:
                self.addr = data[0]
                return build_response(self.addr, 0x84, [self.addr])
            return None
        if func == 0x05:  # CMD_GET_ADDR -> RSP_GET_ADDR(0x85) 1B
            return build_response(self.addr, 0x85, [self.addr])
        return None


def main():
    ap = argparse.ArgumentParser(description="MT6816 编码器从机模拟器")
    ap.add_argument('--addr', type=lambda s: int(s, 0), default=0x01,
                    help='模拟从机地址（默认 0x01）')
    ap.add_argument('--angle', type=lambda s: int(s, 0), default=5000,
                    help='角度原始值 0~16383（默认 5000）')
    ap.add_argument('--voltage', type=lambda s: int(s, 0), default=3300,
                    help='电压 mV（默认 3300）')
    ap.add_argument('--status', type=lambda s: int(s, 0), default=0,
                    help='状态字（默认 0）')
    ap.add_argument('--debug', action='store_true', help='打印收发帧')
    ap.add_argument('--echo', action='store_true',
                    help='模拟半双工单线回显：收到请求先原样回发，再发送真正应答')
    args = ap.parse_args()

    master, slave = pty.openpty()
    tty.setraw(master)
    slave_name = os.ttyname(slave)

    print("MOCK_PORT=%s" % slave_name, flush=True)
    print("模拟从机: 地址=0x%02X 角度=%d 电压=%dmV 状态=0x%X" %
          (args.addr, args.angle, args.voltage, args.status), file=sys.stderr, flush=True)
    print("按 Ctrl+C 退出。请在上位机中打开端口: %s" % slave_name, file=sys.stderr, flush=True)

    s = Slave(args.addr, args.angle, args.voltage, args.status)
    buf = b""
    try:
        while True:
            rlist, _, _ = select.select([master], [], [], 0.2)
            if master in rlist:
                data = os.read(master, 256)
                if not data:
                    break
                buf += data
                if args.debug:
                    print("<< %s" % data.hex(' ').upper(), file=sys.stderr, flush=True)
            while len(buf) >= 6:
                idx = buf.find(FRAME_HEAD)
                if idx < 0:
                    buf = b""
                    break
                if idx > 0:
                    buf = buf[idx:]
                ln = buf[4]
                total = 6 + ln
                if len(buf) < total:
                    break
                frame = buf[:total]
                buf = buf[total:]
                if args.echo:
                    os.write(master, frame)   # 模拟单线回环：原样回发请求帧
                    if args.debug:
                        print(">> [回显] %s" % frame.hex(' ').upper(),
                              file=sys.stderr, flush=True)
                resp = s.handle(frame)
                if resp is not None:
                    os.write(master, resp)
                    if args.debug:
                        print(">> %s" % resp.hex(' ').upper(), file=sys.stderr, flush=True)
    except KeyboardInterrupt:
        print("已退出", file=sys.stderr, flush=True)
    finally:
        os.close(master)
        os.close(slave)


if __name__ == '__main__':
    main()
