# DS18B20 与 CAN 分步调试指南（不改变程序逻辑）

本文使用现有固件，通过 Keil 断点、Watch 窗口、万用表/逻辑分析仪和电脑 CAN 工具，把 DS18B20 与 CAN 分开验证。

## 0. 调试前准备

1. 用 Keil 打开 `MDK-ARM/DS18B20_CAN.uvprojx`。
2. 连接 ST-Link，并给目标板供电。
3. 在 Keil 中执行 Build，然后点击 Download。
4. 点击 Start/Stop Debug Session 进入调试。
5. 如果局部变量在 Watch 中显示 `<not in scope>` 或 `<optimized out>`，在 `Options for Target -> C/C++ -> Optimization` 临时选 Level 0 后重新编译。这只改变调试构建选项，不改变源代码逻辑。

## 1. 只确认 DS18B20 能否读到温度

CAN 线可以暂时不接。程序仍会执行 DS18B20 读取。

### 方法 A：在 main.c 看最终结果

1. 在 `Core/Src/main.c` 第 124 行 `switch (sensor_status)` 设置断点。该位置已经完成一次温度读取。
2. 点击 Run，程序停下后，在 Watch 1 添加：
   - `sensor_status`
   - `raw_temperature`
   - `temperature_milli_c`
3. 正常结果示例：
   - `sensor_status = 0`
   - `raw_temperature = 400`
   - `temperature_milli_c = 25000`
   - 表示 25.000 °C，因为 DS18B20 原始值单位是 1/16 °C。
4. 状态码含义：
   - `0`：读取正常
   - `-1`：没有检测到器件的 Presence 脉冲
   - `-2`：Scratchpad CRC 错误
   - `-3`：温度转换等待超时

### 方法 B：检查 DS18B20 原始 9 字节

1. 在 `Core/Src/ds18b20.c` 第 208 行 CRC 判断处设置断点。
2. 运行到断点后，在 Watch 中添加 `scratchpad`，展开 9 个字节。
3. `scratchpad[0]`、`scratchpad[1]` 是温度低字节和高字节；`scratchpad[8]` 是 CRC。
4. 例如 25 °C 的原始值通常为 `0x0190`，低字节在前，所以前两个字节是 `90 01`。

### 不改代码的硬件检查

- 上电且程序暂停时，PC13/DS18B20 DQ 空闲电压应接近 3.3 V。
- 如果一直是 0 V：检查短路、DS18B20 方向、PC13 焊接和 4.7 kΩ 上拉。
- 如果 `sensor_status=-1`：重点检查 VDD、GND、DQ、4.7 kΩ 上拉及 DS18B20 封装方向。
- 用逻辑分析仪观察 DQ：MCU 复位脉冲低电平约 480 us，释放后 DS18B20 应产生低电平 Presence 脉冲。
- 如果能检测到器件但 CRC 错误，重点检查连线过长、地线、干扰、电源去耦和接触不良。

## 2. 只确认 CAN 能否把数据发到电脑

即使拔掉 DS18B20，当前程序也会发送 CAN 帧。此时 Byte0 应为 1，因而可以完全独立地验证 CAN 链路。

### 电脑 CAN 工具设置

- CAN 2.0 标准帧
- 波特率：500 kbit/s
- 不要使用 Listen-only/Silent 模式；电脑适配器需要参与 ACK
- 接收过滤器设为全部接收，或者只接收标准 ID `0x181`
- CAN_H 对 CAN_H，CAN_L 对 CAN_L，电脑适配器 GND 对板上 DGND

### 在 Keil 中确认待发送内容

1. 在 `Core/Src/can_temperature.c` 第 68 行 `HAL_CAN_AddTxMessage(...)` 设置断点。
2. 运行到断点后，在 Watch 中查看：
   - `header.StdId`，应为十六进制 `0x181`
   - `header.DLC`，应为 8
   - `data`，展开 8 个字节
3. 如果 DS18B20 没接，常见第一帧为：

   `01 00 00 00 00 00 00 00`

   其中 `01` 表示未检测到传感器，第二字节是递增序号。
4. 点击 Run，让程序继续，电脑端应约每秒收到一帧；板上 PE5 LED 每帧翻转一次。

### HAL_OK 不等于电脑已经收到

`HAL_CAN_AddTxMessage()` 返回 HAL_OK 只表示帧已经放入 STM32 的发送邮箱。总线上还必须存在另一个处于正常模式的 CAN 节点来发送 ACK。

如果电脑没有收到，在 Keil Watch 中添加：

- `CAN1->ESR`
- `CAN1->TSR`
- `hcan1.ErrorCode`

`CAN1->ESR` 的 LEC 字段位于 bit 6:4。LEC=3 通常表示 ACK 错误，常见原因是：

- 电脑 CAN 适配器处于只听/静默模式
- CAN_H、CAN_L 接反
- 没有连接公共地
- 两端波特率不一致
- 收发器未供电或接线错误
- 总线终端不正确

### 终端电阻检查

断电后，用万用表测量 CAN_H 与 CAN_L 之间的电阻：

- 约 60 Ω：总线两端各有一个 120 Ω，正常
- 约 120 Ω：只有一个终端
- 明显小于 60 Ω：终端数量过多或可能短路

本板原理图中已经有一个 120 Ω 终端，电脑适配器作为另一端时通常再启用一个 120 Ω 终端即可。

## 3. 推荐调试顺序

1. CAN 线先不接，断点停在 `main.c` 第 124 行，确认温度变量正确。
2. 即使 DS18B20 不正常，也连接电脑 CAN，断点停在 `can_temperature.c` 第 68 行，确认发送数据数组正确。
3. 取消 CAN 发送断点，让程序全速运行，在电脑端确认每秒收到 ID `0x181`。
4. 两部分分别正常后，再联合运行并检查 Byte0=0、Byte2..7 温度数据正确。
