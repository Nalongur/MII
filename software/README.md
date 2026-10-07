# MII 软件

适用硬件：MII V7、ESP32-S3-MINI-1-N8（8 MB Flash，无 PSRAM）、EPD0426A02。

## 开发环境

- 固件：ESP-IDF 5.5.5，C/C++
- Android：Kotlin / Jetpack Compose，JDK 21、Android SDK 36；版本锁定见 `android/gradle/libs.versions.toml`
- 协议：Protobuf / nanopb；版本见 `versions.json`
- 当前构建辅助脚本面向 Windows PowerShell；主机回归还需要 Python、Zig / C++ 工具链和 JVM 依赖

在本目录运行：

```powershell
python tools/validate_baseline.py
powershell -ExecutionPolicy Bypass -File tools/build_host.ps1
powershell -ExecutionPolicy Bypass -File tools/build_firmware.ps1
powershell -ExecutionPolicy Bypass -File tools/build_android.ps1
```

依赖安装完整后，主机与 Android 脚本可加 `-Offline`。固件脚本支持 `-IdfPath`；Android 脚本支持 `-AndroidSdkPath` 和 `-JdkPath`。

## 文档

- [整体设计](docs/MII_整体软件设计说明_v1.0.md)
- [开发与验证流程](workflow/MII_软件开发与验证工作流_v1.0.md)
- [固件](firmware/README.md) / [Android](android/README.md) / [协议](protocol/README.md)
- [依赖与复用决策](docs/reuse-decisions.md)
- [验证记录](verification/W1/2026-10-07-round16-numeric-comparison.md)
- [硬件资料](../hardware/README.md)

固件业务通信仍受隔离与认证门控限制，BLE 默认关闭。Android 当前为未绑定/空状态基础界面，尚未接入实际无线同步。历史测试报告只证明报告对应的检查点；本次公开整理未重新完成整套编译或实物验收。

项目自有软件与软件文档采用 [MIT](../LICENSES/MIT.txt)，第三方内容保留原许可。
