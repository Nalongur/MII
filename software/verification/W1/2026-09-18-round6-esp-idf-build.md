# W1 第六轮 ESP-IDF 固件构建检查点

- 状态：`CHECKPOINTED`（Debug/Release 镜像构建通过，尚未烧录或实机验证）
- 开发窗口：2026-09-18 13:25–13:54（Asia/Shanghai）
- 起点：`a9f04d3 build: integrate nanopb firmware component`
- 工具链：ESP-IDF v5.5.5，ESP32-S3 目标工具集
- 检查点：包含本报告的提交与 Git 标签 `checkpoint/round6-esp-idf-build-20260918`

## 本轮完成

- 从 Espressif 官方仓库检出 ESP-IDF `v5.5.5`，提交 `b774170ff46c393eeb5e495ea37936038d3f4f4f`。
- 使用官方 `install.ps1 esp32s3` 安装目标工具链和隔离 Python 环境。
- 在 `versions.json` 中锁定 `espIdfVersion`，静态校验会拒绝版本漂移。
- 新增 `tools/build_firmware.ps1`，验证精确版本后构建 Debug、Release 或两者。
- 两套构建均解析并编译 `board_v7`、`mii_protocol`、提交的 nanopb 生成代码和 runtime。
- 分区表在 8 MB Flash 边界内生成，两个 OTA 槽均为 1920 KiB。

## 构建证据

| 检查 | Debug | Release |
|---|---:|---:|
| ESP-IDF 构建 | 退出码 0 | 退出码 0 |
| `mii_firmware.bin` | 195,856 B (`0x2fd10`) | 175,520 B (`0x2ada0`) |
| OTA 槽剩余 | 1,770,224 B（约 90%） | 1,790,560 B（约 91%） |
| `bootloader.bin` | 20,832 B | 20,832 B |
| 引导区剩余 | 11,936 B（36%） | 11,936 B（36%） |
| 8 MB Flash / 无 PSRAM | 已确认生成配置 | 已确认生成配置 |
| 编译策略 | `CONFIG_COMPILER_OPTIMIZATION_DEBUG=y` | `CONFIG_COMPILER_OPTIMIZATION_SIZE=y` |

Release 应用镜像比 Debug 小 20,336 B（约 10.38%）。构建目录属于本地产物并由 `.gitignore` 排除，不提交二进制。

## 验证边界

- 本轮证明了 ESP32-S3 Xtensa 编译、ESP-IDF 组件解析、最终链接、镜像生成和分区尺寸检查。
- 没有连接、烧录或启动 MII V7，因此没有证明 PSHOLD 时序、启动日志、GPIO 安全电平或外设行为。
- 没有构建 Android APK，也没有进行 BLE、Wi-Fi、OTA 或手机端到端测试。
- `mii_data` 仍是 W1 容量占位区；其内部格式和子分区必须在后续阶段冻结。

## 准确续作顺序

1. 在固件目标端增加最小协议编解码自检，并保持主机固定向量一致。
2. 完成 W0 实物身份和关键电平复核后，再进行首次受控烧录。
3. 上电先测 PSHOLD、GNSS_EN、EPD CS 和 LED 安全电平，再进入驱动开发。
4. 之后按 BME280、GNSS、EPD 的风险顺序逐个接入和做实机证据记录。
