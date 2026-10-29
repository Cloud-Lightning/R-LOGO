# STM32F072C8T6 CAN 实机验收记录

日期：2026-10-29。

**后续状态更新：** 用户接入灯板后，完整图案测试期间出现 ST-Link 掉线及 CAN 无回包。现已切换到独立的 1/3 颗低亮度测试固件，完成烧录校验、CAN/DMA/输出缓冲检查，用户现场确认“全灭 → 1 颗红灯 → 全灭 → 3 颗蓝灯 → 全灭”正确。**板内当前运行小负载版，`03`=1 红、`04`=3 蓝、`00`=全灭。** 具体结果见 [小负载实机测试记录](artifacts/led-low-load/README.md)。以下保留此前完整图案固件的 CAN 验收记录，原固件仍保存在 `artifacts/can-bringup/`。

结论：本机连接的 USBCAN-II CAN1 与 STM32F072C8T6 已在 **1 Mbps、正常模式、标准帧**下完成真实双向通信。最终固件已写入板子，下载校验通过，随后从 Flash 回读的固件与交付 BIN 的 SHA-256 一致。测试不是 MCU 内部回环，也不是 CAN 盒子自收自发。

## 现场设备与最终配置

| 项目 | 实测或配置 |
|---|---|
| MCU | STM32F07x，设备 ID `0x448`，64 KB Flash；工程目标 STM32F072C8T6 |
| ST-Link | `37FF71064E57343610991843`，检测目标电压 3.30 V |
| CAN 盒子 | 官方 DLL 返回 `USBCAN-II`，序列号 `31F0002C642`，双通道，固件版本 `0x341` |
| 已验证接口 | 盒子 **CAN1**，官方 API 的 `CANInd=0`；CAN2 未测试 |
| MCU 引脚 | PA11 / AF4 = CAN_RX；PA12 / AF4 = CAN_TX |
| 时钟及速率 | HSE 8 MHz → PLL ×6 → PCLK1 48 MHz；1 Mbps |
| 位定时 | Prescaler=3，BS1=11 TQ，BS2=4 TQ，SJW=4 TQ，采样点 75% |
| MCU 模式 | `CAN_MODE_NORMAL`；实读 BTR=`0x033A0002`，LBKM/SILM 均为 0 |
| CAN 盒子模式 | Mode=0 正常模式；Timing0=`0x00`，Timing1=`0x14` |
| 自动恢复配置 | 自动退出 bus-off、自动重发已开启；本轮没有进行断线或 bus-off 注入测试 |

最终寄存器实读：CAN MCR=`0x00010040`，BTR=`0x033A0002`，ESR=`0x00000000`。ESR 中 TEC、REC 和错误状态均为 0。

## 原程序的现场状态

首次接入时，CPU 的 PC=`0x08001CE4`、LR=`0xFFFFFFF9`、XPSR=`0x41000003`，IPSR=3，处在 HardFault 异常内。原 Flash 也不匹配目录中已有的 `build/Debug/F072.bin`。

先备份完整 64 KB Flash 为 `artifacts/can-bringup/flash_before.bin`。备份 SHA-256：

```text
cea63b5fa90995670871f51ef382ed340fc00f89fbc55e6ca2cce13a79bf5a0d
```

从当前源码重新编译并下载后，在修改位定时前，盒子发送的两帧 `0x002 / 03` 已被 MCU 正确接收，接收计数增加 2，显示函数执行计数增加 1。由此确认现有接线能够通信。原固件为什么进入 HardFault 尚未追溯；不能把这次原始故障归因于 SJW。后续的 SJW、错误统计和自动恢复配置属于可靠性补充。

## 源码修改

- `Core/Src/can.c`：SJW 改为 4 TQ；开启自动退出 bus-off 和自动重发；检查中断使能返回值；加入发送完成、错误和回包队列丢弃计数。
- 保留原灯控协议，仅接受标准数据帧、非空数据及 `03/04` 命令，避免扩展帧、远程帧、空帧或无效数据误触发。
- 增加独立诊断请求 `0x600` 与回复 `0x601`，逐字节回传 DLC 及数据，方便在 CAN 工具中直接证明收发链路。原灯控 ID `0x002` 不自动回包。
- `Core/Src/main.c`：主循环调用 `CAN_Process()`；LED DMA 忙时继续处理主循环，显示计数只在实际启动成功后增加。
- `F072.ioc`：同步位定时及自动恢复设置。CAN NVIC 仍由 `can.c` 的 USER CODE 初始化段手工启用，`stm32f0xx_it.c` 的 USER CODE 段保留中断处理函数，不重复生成第二个处理函数。

## 编译、烧录及回读

- GCC/CMake Debug 构建成功：Flash 18,220 B；链接器 RAM 使用量 14,920 B / 16,384 B，已包含预留 2 KB 栈。HAL 库初次全编译有 3 条原有未使用参数警告，应用代码无新增警告。
- Keil ARMCC 工程也重新构建成功：**0 Error(s), 0 Warning(s)**，日志为 `artifacts/can-bringup/keil_build.log`。Keil 产物已更新，但本轮板上实际烧录并验收的是 GCC 产物。
- STM32CubeProgrammer 输出 `Download verified successfully`，执行复位及运行成功。
- 最终从地址 `0x08000000` 回读 18,220 B，与交付 `F072_CAN_1M.bin` 完全一致。二者 SHA-256：

```text
1e81caadff92c838a32dd3a208a5a468e2de701813cba5b18f2f1c918091845f
```

当前已验证固件：

- `artifacts/can-bringup/F072_CAN_1M.hex`
- `artifacts/can-bringup/F072_CAN_1M.bin`
- `artifacts/can-bringup/F072_CAN_1M.elf`

完整日志：`flash_verify.log`、`final_readback.log`、`manifest.json`。

## 实际 CAN 收发结果

| 验证项 | 结果 |
|---|---|
| 首轮双向收发 | 10/10；同时发送原灯控命令，MCU 灯控命令计数和显示启动计数均增加 10 |
| DLC=0～8 的诊断请求 | 9/9，回复长度和内容一致 |
| 连续带序号诊断请求 | **1000/1000**；逐帧严格核对内容，耗时约 17.3 秒 |
| 非法/无关帧 | 7/7 按预期忽略，不回包、不触发灯控 |
| MCU 接收统计 | 正式连续验收阶段增加 1016：1000 连续请求 + 9 个 DLC 测试 + 7 个无效/无关帧 |
| MCU 发送完成统计 | 同阶段增加 1009，与 1000 + 9 个应答完全一致 |
| CAN 错误/回包丢弃 | 增量均为 0；ESR=0 |
| 复位后复测 | 20/20；MCU 接收=20、发送完成=20、错误=0、回包丢弃=0 |
| 程序运行 | core running，SysTick 持续增加 |

完整逐帧数据和 ELF 动态解析的 SRAM 地址在 `artifacts/can-bringup/acceptance.json`；快照日志为 `acceptance_before.log`、`acceptance_after.log`、`acceptance_after_reset.log`。

灯板的实际光学显示未验收：用户当前看不到灯板，本次以真实 CAN 回传、MCU 计数及运行状态验收。主程序复位后不主动设置灯色。

## 在 CAN 软件中直接复测

选择 **USBCAN-II → CAN1 → 1 Mbps → 正常模式 → 标准数据帧**。

| 用途 | 发送 ID | DLC | 发送数据（HEX） | 预期 |
|---|---|---|---|---|
| 红色 R 命令 | `002` | 1 | `03` | 执行红色显示命令，不自动回包 |
| 蓝色 R 命令 | `002` | 1 | `04` | 执行蓝色显示命令，不自动回包 |
| 双向通信验证 | `600` | 8 | `11 22 33 44 55 66 77 88` | 收到标准数据帧 `601`，DLC=8，数据完全相同 |

命令行复测（从工程根目录运行，先关闭占用盒子的其他 CAN 软件）：

```powershell
# 发送 100 个请求，并逐帧核对 0x601 回复；不改变灯色
py tools\can_box.py --channel 0 --roundtrip --count 100 --log artifacts\can-bringup\manual_test.json

# 完整硬件验收：需要 CAN 盒子和 ST-Link 同时接入
# 从实际 ELF 解析变量地址；测试末尾会复位 MCU
py tools\verify_can.py --count 1000

# 使用当前源码重新构建本轮采用的 GCC 工程
cmake -S . -B build/can-bringup -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/can-bringup -j 4
```

官方 64 位 DLL、头文件和示例已从用户给定的资料压缩包提取到 `tools/can_vendor/`，脚本直接调用它们，无需安装额外 Python CAN 库。脚本使用单次发送模式；只有收到 STM32 的匹配回复才算双向测试通过，DLL 的发送返回值本身不作为通信成功证据。
