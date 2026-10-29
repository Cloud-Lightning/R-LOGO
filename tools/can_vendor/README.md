# USBCAN-II 官方库（本地依赖）

此目录不发布官方 DLL 或厂家示例。请使用自己的官方 CAN 分析仪资料包提取 Windows x64 `ControlCAN.dll`：

```powershell
py tools/extract_can_vendor.py --archive "CAN分析仪资料20200101_顶配.rar"
```

成功后保存为 `tools/can_vendor/x64/ControlCAN.dll`，供 `can_box.py` 加载。

若手工放置 DLL，也应放进名为 `x64` 的子目录。需要先安装盒子的官方 WinUSB 驱动。不同位数的 DLL 不能混用。
