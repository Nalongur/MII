# W1 第七轮固件协议编解码检查点

- 状态：`CHECKPOINTED`（主机回归与 Debug/Release 镜像构建通过，尚未烧录或实机验证）
- 开发窗口：2026-09-19 00:10–00:42（Asia/Shanghai）
- 起点：`c52f614 build: pin ESP-IDF and verify firmware images`
- 版本：固件 `0.1.0-dev.2`，协议 `1.0.0`，ESP-IDF `5.5.5`
- 检查点：包含本报告的提交与 Git 标签 `checkpoint/round7-firmware-protocol-codec-20260919`

## 本轮完成

- 增加无堆分配的 C++17 固件协议 API，首先覆盖 `DeviceStatusRequest` Envelope 编码与解码。
- 对设备 ID、输出缓冲区、截断输入、协议版本和 payload 类型建立显式错误边界。
- 主机测试直接编译固件中的编解码源码、提交的 nanopb 生成类型和 runtime，不维护第二份实现。
- 增加 31 字节固定向量自检，同时验证未知字段 127 可跳过。
- Debug 配置在 PSHOLD 和其他安全输出初始化后执行自检；Release 配置不编译自检源码。
- 固件构建包装器新增 `-Fresh`，可清理生成配置，并在构建后拒绝 Debug/Release 自检开关串档。

## 验证证据

| 检查 | 结果 |
|---|---|
| 静态基线 | 126 项通过 |
| 独立 C++ wire 向量 | 31 字节一致 |
| nanopb 生成/提交产物防漂移 | 通过 |
| 固件真实 codec 编解码 | 通过 |
| 边界错误（小缓冲区、超长 ID、截断 wire、不支持版本） | 通过 |
| 未知字段 127 | 通过 |
| Kotlin/JVM 生成类型向量与未知字段 | 通过 |
| ESP-IDF Debug/Release 编译、链接和镜像生成 | 通过 |

| 镜像 | Debug | Release |
|---|---:|---:|
| `mii_firmware.bin` | 206,480 B (`0x32690`) | 175,520 B (`0x2ada0`) |
| OTA 槽剩余 | 1,759,600 B（约 89%） | 1,790,560 B（约 91%） |
| `bootloader.bin` | 20,832 B | 20,832 B |
| 协议启动自检 | 编译并链接 | 关闭，源文件不编译 |

Debug 相比第六轮增加 10,624 B，包含编解码与自检路径。Release 仍为 175,520 B：编解码组件可编译，但未被 Release 主程序引用的段由链接器移除。

## 验证边界

- 本轮证明了固件编解码源码在主机与 ESP32-S3 Xtensa 工具链上可编译，且 Debug 镜像已链接启动调用。
- 没有连接、烧录或启动 MII V7，因此没有证明目标端自检实际运行、启动日志或 GPIO 电平。
- 没有建立 Android APK，也没有进行 BLE、Wi-Fi、OTA 或手机端到端测试。

## 准确续作顺序

1. 完成 W0 实物身份、供电与关键电平复核后，再进行首次受控烧录。
2. 捕获 Debug 启动日志，确认协议自检实际通过，并测量 PSHOLD、GNSS_EN、EPD CS 和 LED 安全电平。
3. 为真实传输层设计会话内 `message_id`/`sequence_number` 状态和分帧边界。
4. 随后建立 Android 最小工程，复用当前 Kotlin 生成配置和固定向量。
