# 第三方组件

## STM32 HAL 与 CMSIS

仓库保留原工程中的第三方驱动及许可证，不改变其授权条款：

- `Drivers/STM32F0xx_HAL_Driver/LICENSE.txt`：按原组件说明适用软件包许可证，若未附软件包授权条款，则适用 BSD-3-Clause。
- `Drivers/CMSIS/LICENSE.txt`：Apache License 2.0。
- `Drivers/CMSIS/Device/ST/STM32F0xx/LICENSE.txt`：按原组件说明适用软件包许可证，若未附软件包授权条款，则适用 Apache-2.0。

## CAN 盒子官方库

`ControlCAN.dll`、头文件及厂家示例来自用户提供的 USBCAN-II 官方资料包。项目未取得额外的重发布授权，因此发布包不包含这些二进制库。使用者应从自己的官方资料包中提取与 Python 位数相匹配的库，并遵守厂家的授权条款。

本项目的 `tools/extract_can_vendor.py` 仅在本机解包所选 DLL 和头文件，输出到已被 Git 忽略的 `tools/can_vendor/`。Python CAN 脚本使用官方 x64 DLL，不需要额外安装 Python CAN 库。
