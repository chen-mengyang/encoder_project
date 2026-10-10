# uart_half_duplex_g030f6

基于 STM32G030F6 的 UART 半双工通信工程（STM32CubeIDE）。

## 功能

- UART 半双工协议通信（协议说明见 [docs/protocol.md](docs/protocol.md)）
- MT6816 磁编码器读取（SPI）
- 24C02 EEPROM 读写（I2C）
- ADC 电压采集 / LED / GPIO / TIM 等

## 环境

- MCU：STM32G030F6P6
- IDE：STM32CubeIDE
- HAL：STM32G0xx HAL

## 构建说明

用 STM32CubeIDE 导入本工程后，`Debug/` 目录会在编译时自动生成（已被 `.gitignore` 忽略，不入库）。
