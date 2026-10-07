# W1 第三轮生成代码构建检查点

- 状态：`CHECKPOINTED`（Kotlin 生成代码构建通过，W1 总退出门槛尚未通过）
- 开发窗口：2026-09-18 09:49–10:04（Asia/Shanghai，15 分钟）
- 基线提交：`4adccd4 build: add C++ and Kotlin protocol conformance`
- 检查点：包含本报告的提交与 Git 标签 `checkpoint/round3-generated-protobuf-20260918`

## 本轮完成

- 固定 protobuf Gradle 插件 0.9.4、protoc 3.24.4 和 protobuf-java 3.24.4。
- 构建时把规范 `protocol/mii.proto` 暂存到 Gradle 构建目录，再运行 protoc；仓库不保存第二份 schema 或生成源码。
- Kotlin 测试改用 `com.mii.protocol.v1` 生成消息类型，不再手写字段编码。
- 生成类型编码结果与既有 C++ 独立编码器及固定向量逐字节一致。
- 生成类型成功反序列化固定向量，并保留合成的未来字段 127，证明当前 Java/Kotlin reader 可安全读取带未知字段的 v1 消息。
- nanopb 0.4.9.1 严格生成预检发现原 `mii.options` 的短字段名未匹配；已改为 package-qualified 字段名，预检随后生成 33 字节 `device_id` 缓冲和 512 字节 `payload` 上限。

## 构建证据

固定输出仍为 31 字节：

```text
0a02080110011a0d4d49492d544553542d303030312001280130013a020801
```

| 命令/检查 | 结果 |
|---|---|
| `gradlew.bat --no-daemon --offline clean check` | `BUILD SUCCESSFUL`；12 个任务实际执行，含 `generateProto`、Kotlin/Java 编译与运行 |
| `tools\build_host.ps1 -Offline` | 退出码 0；C++ 与 Kotlin 生成类型输出相同 31 字节 hex |
| 未知字段测试 | 生成 parser 保留字段 127，已知字段和内嵌 payload 均正确解码 |
| nanopb 0.4.9.1 `--error-on-unmatched` 预检 | 成功生成 `mii.pb.[ch]`，两条容量 option 均匹配 |

最初直接把 `protocol` 目录作为 proto source root，Gradle 会扫描其中的 `.gradle` 锁文件，导致输入哈希失败；改为由显式 Sync 任务只暂存规范文件。随后为 `processResources` 补充显式任务依赖，避免不确定的任务顺序。这两次失败均发生在最终成功构建之前，未作为通过证据。

## 验证边界

- 本轮生成的是 Java protobuf 消息类型，由 Kotlin/JVM 调用；不是 Android APK，也未在 Android Runtime 运行。
- C++ 端仍是独立的最小 wire 编码器；尚未从 schema 生成或链接 nanopb C/C++ 类型。
- 当前用户环境安装了 nanopb generator 0.4.9.1 供预检，但 runtime 源码与可复现安装尚未纳入仓库；该预检不计作 nanopb 编译通过。
- 本轮没有构建 ESP-IDF 固件、烧录设备，也没有实物板或手机验证。
- W0 实物身份记录仍为 `PENDING`。

## 准确续作顺序

1. 固定 nanopb generator/runtime，生成 C/C++ 类型并复用同一固定向量做 encode/decode/未知字段测试。
2. 把 nanopb 生成与协议测试接入 ESP-IDF 组件和 Debug/Release 构建。
3. 锁定 Android SDK、AGP 与 Compose 兼容矩阵，建立最小 Android Debug 工程并复用当前生成配置。
4. 完成 W0 实物身份与关键电平复核后，才进入烧录和板级测量。
