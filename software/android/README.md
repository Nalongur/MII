# MII Android

最小 Kotlin/Compose 工程，命名空间 `com.mii.app`（Debug 为 `.debug`）。
设备、设计、数据、设置四个入口只显示未绑定/空状态，没有伪造数据。
目前未接入无线扫描、配对、Keystore、数据库或实际同步。

## 固定工具链

- Gradle 8.11.1，复用现有已锁 SHA-256 wrapper；AGP 8.10.1。
- Kotlin/Compose compiler plugin 2.1.10；Compose BOM 2025.02.00
  （UI/runtime/foundation 1.7.8，Material3 1.3.1）；Activity Compose 1.10.1。
- compile/target SDK 36、min SDK 26、build-tools 36.0.0。
- 本机 JDK 21 执行 Gradle，Java/Kotlin bytecode 17。
- 复用 protobuf plugin 0.9.4、protoc 3.24.4，Android javalite 3.24.4。
  只从 `../protocol/mii.proto` 生成，不保存第二份 schema/生成类型。

实施前官方/GitHub来源、许可和采用理由见 `../docs/reuse-decisions.md`。
Gradle/AGP/Kotlin/AndroidX/protobuf plugin 为 Apache-2.0，protobuf runtime/protoc
为 BSD-3-Clause，JUnit 4.13.2 为 EPL-1.0；无样例 App 整体移植。

## 构建

在软件仓库根运行：

```powershell
powershell -ExecutionPolicy Bypass -File tools/build_android.ps1
# 依赖缓存完整后可添加 -Offline；路径可用 -AndroidSdkPath / -JdkPath 指定。
```

脚本执行 Android 单元向量测试、warningsAsErrors lint、Debug 和压缩 Release。
Debug 使用本地开发签名，Release 输出未签名 APK，不借用 Debug 密钥充当发布签名。
应用版本来自 `../versions.json`。默认备份与设备转移禁用；凭据后续使用 no-backup
私有目录与不可导出的 Keystore key，不允许通过手机备份静默迁移绑定密钥。

测试直接 staging 既有 `AuthVectorCheck.kt` 和真实固定向量，校验八类认证及 HMAC；
Android lite 单独核对旧 31 B Envelope 和未知字段保留。JVM 单元测试不等于 Android
Keystore、界面、手机配对或协议端到端验证。未安装、启动模拟器或操作手机。
