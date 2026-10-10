# encoder_project

编码器配置工具项目：STM32 单片机固件 + Qt 上位机，两者通过 UART 半双工协议通信。

## 目录结构

| 目录 | 内容 | 环境 |
|---|---|---|
| [`firmware/`](firmware/) | STM32G030F6 固件（MT6816 磁编码器、24C02 EEPROM、UART 半双工协议） | STM32CubeIDE |
| [`host/`](host/) | Qt 上位机 `encoder_config`（串口配置、仪表显示） | Qt / qmake |

## 通信协议

协议说明见各端文档：

- 固件端：[firmware/docs/protocol.md](firmware/docs/protocol.md)
- 上位机端：[host/protocol.md](host/protocol.md)

调试时可用 `host/tools/mock_slave.py` 模拟从机。

## 构建

### 固件

用 STM32CubeIDE 导入 `firmware/`，编译生成的 `Debug/` 已被 `.gitignore` 忽略。

### 上位机

```bash
cd host
qmake encoder_config.pro
make
```

编译产物（`build/ moc/ obj/ ui/ rcc/ .qm/ Makefile`）均不入库。
