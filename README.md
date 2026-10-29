# R-LOGO · STM32F072C8T6 CAN 灯板控制

STM32F072C8T6 通过 CAN 接收控制命令，使用 PA8 / TIM1 PWM + DMA 驱动 16×16 WS2812 灯板。CAN 使用 PA11（RX）、PA12（TX），1 Mbps，正常模式、标准帧。

## 当前已验证版本

2026-10-29 已完成 CAN 双向收发及小负载灯板实机验证。**当前板内运行的是 1 颗 / 3 颗低亮度测试版，最后状态为全灭。**

| 验证项 | 结果 |
|---|---|
| CAN 带序号连续请求与回传 | 1000/1000 内容一致 |
| DLC 0～8 字节 | 9/9 通过 |
| 复位后 CAN 双向收发 | 20/20 通过 |
| 小负载灯板 | 全灭 → 1 红 → 全灭 → 3 蓝 → 全灭，现场确认正确 |
| 小负载数据检查 | 完整 PWM 缓冲非零像素数量为 0、1、0、3、0 |
| 小负载期间 CAN / SWD | CAN 错误为 0，ST-Link 保持连接 |

完整图案测试期间曾出现 ST-Link 掉线和 CAN 无回包，小负载测试未复现。供电随负载变化是优先排查方向，尚未测量亮灯瞬态电压，不能据此确定电源额定容量不足。

## 固件与命令

CAN 盒子选 USBCAN-II / CAN1（API 通道 0）、1 Mbps、正常模式、标准数据帧。灯控 ID 为 `0x002`，DLC=1。以下均为十六进制数据：

| 数据 | 已验证小负载版 | 普通完整图案版 |
|---|---|---|
| `00` | 全灭 | 忽略 |
| `03` | 仅第 1 颗低亮度红灯 | 红色 R 图案 |
| `04` | 仅前 3 颗低亮度蓝灯 | 蓝色 R 图案 |

诊断请求 ID `0x600`，DLC=0～8；MCU 用 ID `0x601` 原样回传长度和数据。灯控 ID `0x002` 本身不自动回包。

- [当前实机小负载固件及记录](artifacts/led-low-load/README.md)：`F072_CAN_1LED_3LED.hex` / `.bin` / `.elf`。
- [CAN 验收报告](CAN_调通_验收报告.md)：包括此前完整图案固件的 CAN 验收和后续小负载更新。
- `artifacts/can-bringup/F072_CAN_1M.*`：此前完成 1000 轮 CAN 验收的原始固件快照。它与后续源码重编译产物不保证逐字节相同，验收脚本应使用相匹配的 ELF。

小负载版本上电全灭，亮灯通道值为 16/255。每次仍发送完整 256 像素帧，未选中的灯均发送零值，避免保留旧颜色。复用原 DMA 缓冲，不另开完整 RGB 帧缓冲。

## 编译与下载

需要 CMake 3.22+、Ninja、GNU Arm Embedded 工具链，并将 `arm-none-eabi-*` 加入 PATH。推荐先使用小负载版：

```powershell
cmake -S . -B build/led-low-load -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLED_LOW_LOAD_TEST=ON
cmake --build build/led-low-load -j 4
```

下载已验证的小负载固件，需要 STM32CubeProgrammer CLI 和已接好的 SWD：

```powershell
STM32_Programmer_CLI -c port=SWD freq=1000 mode=UR -w artifacts/led-low-load/F072_CAN_1LED_3LED.elf -v -rst -run
```

普通完整图案版构建（不会自动烧录或点灯）：

```powershell
cmake -S . -B build/can-bringup -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLED_LOW_LOAD_TEST=OFF
cmake --build build/can-bringup -j 4
```

Keil 工程位于 `MDK-ARM/F072.uvprojx`，默认编译普通完整图案版。如需在 Keil 中编译小负载版本，需在 C/C++ 预处理宏中增加 `LED_LOW_LOAD_TEST=1`。

## CAN 盒子与测试脚本

Windows 64 位 Python，使用盒子官方 `ControlCAN.dll`。此仓库提供本地提取脚本；官方压缩包和 DLL 不随项目发布。

```powershell
py tools/extract_can_vendor.py --archive "官方 CAN 分析仪资料压缩包.rar"
```

盒子测试前关闭其他占用 CAN 设备的软件。

```powershell
# CAN 双向收发，不改变灯色
py tools/can_box.py --channel 0 --roundtrip --count 100

# 仅适用于已烧录本项目小负载固件；会实际点亮 1/3 颗灯，最后全灭
py tools/test_led_low_load.py --hold 5

# 完整 CAN 验收，默认要求板内匹配此前 1000 轮验收的固件快照；末尾复位 MCU
py tools/verify_can.py --count 1000
```

`test_led_low_load.py` 会先核对 Flash 与专用 BIN 一致，再检查 CAN 回包、MCU 计数、DMA 完成及完整 PWM 缓冲。实际亮灯颗数和颜色仍需现场确认。

## 目录与依赖

- `Core/`：应用与外设初始化。
- `Drivers/`：CMSIS 与 STM32 HAL，保留原有许可证。
- `cmake/`、`CMakeLists.txt`、`F072.ioc`：GCC 构建与 CubeMX 配置。
- `MDK-ARM/`：Keil 工程与启动文件。
- `tools/`：CAN 盒子收发与实机验收脚本。
- `artifacts/`：已验证固件、验收结果及必要日志。

第三方依赖说明见 [THIRD_PARTY.md](THIRD_PARTY.md)。构建缓存、IDE 会话、原始未知 Flash 备份和本地凭据不进入发布包。
