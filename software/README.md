# MII 软件

适用硬件：MII V7、ESP32-S3-MINI-1-N8（8 MB Flash，无 PSRAM）、EPD0426A02。

## 开发环境

- 固件：ESP-IDF 5.5.5，C/C++
- Android：Kotlin / Jetpack Compose，JDK 21、Android SDK 36；版本锁定见 `android/gradle/libs.versions.toml`
- 协议：Protobuf / nanopb；版本见 `versions.json`
- 当前构建辅助脚本面向 Windows PowerShell；主机回归还需要 Python、Zig / C++ 工具链和 JVM 依赖

先安装上述工具链，并让 `python`、`zig` 和 JDK 21 可用。主机协议生成器依赖安装命令：

```powershell
python -m pip install --user -r protocol/host-tests/nanopb/requirements.txt
```

在本目录运行：

```powershell
python tools/validate_baseline.py
powershell -ExecutionPolicy Bypass -File tools/build_host.ps1
powershell -ExecutionPolicy Bypass -File tools/build_firmware.ps1
powershell -ExecutionPolicy Bypass -File tools/build_android.ps1
```

依赖安装完整后，主机与 Android 脚本可加 `-Offline`。固件脚本支持 `-IdfPath`；Android 脚本支持 `-AndroidSdkPath` 和 `-JdkPath`，默认从 `JAVA_HOME` 读取 JDK 路径。

## 文档

- [固件](firmware/README.md) / [Android](android/README.md) / [协议](protocol/README.md)
- [第三方依赖与许可](../docs/third-party.md)
- [硬件资料](../hardware/README.md)

固件业务通信仍受隔离与认证门控限制，BLE 默认关闭。Android 当前为未绑定/空状态基础界面，尚未接入实际无线同步。构建成功不代表实物板、无线互通或整机功能已验证；目前没有正式安装包。

项目自有软件与软件文档采用 [MIT](../LICENSES/MIT.txt)，第三方内容保留原许可。
