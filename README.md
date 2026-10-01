# STM32F407VET6 DS18B20 + CAN

本工程由 STM32CubeMX 6.14.0 生成，目标工具链为 MDK-ARM / Keil µVision 5。

## 硬件配置

- MCU: STM32F407VET6, LQFP100
- 系统时钟: 168 MHz（HSE 8 MHz，PLLM=4，PLLN=168，PLLP=2，PLLQ=7）
- DS18B20 DQ: PC13，开漏输出，板载 4.7 kΩ 上拉到 3.3 V
- CAN1 RX: PB8, AF9
- CAN1 TX: PB9, AF9
- CAN 收发器: SN65HVD232
- CAN 波特率: 500 kbit/s（APB1=42 MHz，Prescaler=6，BS1=11 TQ，BS2=2 TQ，SJW=1 TQ）
- LED: PE5，每发送一帧翻转一次

## CAN 数据帧

- Standard ID: 0x181
- DLC: 8
- Byte 0: 状态，0=正常，1=未检测到传感器，2=CRC错误，3=转换超时
- Byte 1: 递增序号
- Byte 2..3: DS18B20 原始温度，int16，小端序，单位 1/16 °C
- Byte 4..7: 温度，int32，小端序，单位 0.001 °C

例如 Byte4..7 解码为 25375，表示 25.375 °C。

## 主要文件

- `Core/Src/ds18b20.c`: 单总线时序、CRC8、温度读取
- `Core/Src/can_temperature.c`: CAN 过滤器、启动和温度帧发送
- `Core/Src/main.c`: 每 1 秒读取并发送一次
- `MDK-ARM/DS18B20_CAN.uvprojx`: Keil 工程

注意：板上已有 120 Ω 终端电阻。如果电脑端 CAN 适配器也处于总线另一端，应保留适配器端的 120 Ω；整条总线通常只在两端各接一个终端。
