# W1 第二轮主机构建检查点

- 状态：`CHECKPOINTED`（主机构建通过，W1 总退出门槛尚未通过）
- 开发窗口：2026-09-12 23:42–2026-09-13 00:11（Asia/Shanghai，29 分钟）
- 基线提交：`0cbb969 feat: add W1 lightweight firmware and protocol baseline`
- 检查点：包含本报告的提交与 Git 标签 `checkpoint/round2-host-build-20260913`

## 本轮完成

- 通过 WinGet 安装当前用户级 Zig 0.16.0 便携工具链；安装包来自 `ziglang.org`，WinGet 报告哈希校验通过。
- 新增 C++17 协议固定向量程序，由 `zig c++` 实际编译和执行。
- 新增 Kotlin/JVM 2.1.10 主机构建，使用 protobuf-java 3.24.4 的 `CodedOutputStream` 编码同一向量。
- 生成 Gradle Wrapper 8.11.1，并在 wrapper 配置中固定官方发行包 SHA-256。
- 新增 `tools/build_host.ps1`，串行执行静态基线、C++ 构建/运行和 Kotlin 构建/运行。
- 主机生成物统一忽略，不把 EXE、Gradle 缓存或构建目录提交到 Git。

## 工具链快照

| 项目 | 本轮版本/状态 |
|---|---|
| Zig | 0.16.0，WinGet 用户级便携安装 |
| C++ | C++17，经 `zig c++` 构建 |
| Gradle Wrapper | 8.11.1 |
| Kotlin/JVM | 2.1.10 |
| protobuf-java | 3.24.4 |
| Java | Temurin 21.0.11 |
| ESP-IDF / CMake | 未安装或未进入 PATH |
| Android SDK / adb | 未安装或未进入 PATH |
| protoc / nanopb generator | 未安装或未进入 PATH |

## 构建证据

固定输出均为 31 字节：

```text
0a02080110011a0d4d49492d544553542d303030312001280130013a020801
```

| 命令/检查 | 结果 |
|---|---|
| `python tools\validate_baseline.py` | 76 项静态规则通过 |
| `tools\build_host.ps1 -Offline` | 退出码 0；C++ 与 Kotlin 均输出 PASS 和相同 hex |
| `gradlew.bat --no-daemon --offline clean check` | `BUILD SUCCESSFUL`；4 个任务实际执行 |
| 首次联网 Kotlin 构建 | `BUILD SUCCESSFUL`；用于补齐缺少的 Kotlin stdlib 缓存 |

首次 `-Offline` 尝试因本机缺少 `kotlin-stdlib:2.1.10` 缓存而失败；允许 Gradle 联网补齐一次后，完整离线构建与离线 clean build 均通过。首次 Zig 构建生成标准库缓存时出现来自 libc++ 头文件的空指针标注警告，已在主机构建入口中只屏蔽该第三方警告；项目代码仍保留 `-Wall -Wextra -Wpedantic`。

## 验证边界

- C++ 端当前是独立的最小 wire 编码器；Kotlin 端使用正式 protobuf-java 编码 API。
- 两端都还没有使用从 `mii.proto` 生成的 nanopb/Kotlin 类型，因此这次通过不能代替 W1 最终生成代码一致性门槛。
- 本轮没有构建 ESP-IDF 固件、Android APK，没有烧录，也没有实物板或手机验证。
- W0 实物身份记录仍为 `PENDING`。

## 准确续作顺序

1. 锁定 protoc 与 nanopb generator 版本，生成 C/C++ 和 Kotlin 类型；生成物必须可由干净环境重复得到。
2. 把当前两个手写字段编号测试替换或补充为生成类型的双向编解码测试，继续复用同一固定向量。
3. 锁定 Android SDK、AGP 与 Compose 兼容矩阵，建立最小 Android Debug 工程并构建 APK。
4. 锁定 ESP-IDF 具体版本，构建 Debug/Release、生成分区表并记录镜像大小。
5. 完成 W0 实物身份与关键电平复核后，才进入烧录和板级测量。
