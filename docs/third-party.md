# 第三方依赖与许可

| 依赖 | 固定版本 / 来源 | 许可 |
| --- | --- | --- |
| nanopb runtime | [0.4.9.1](https://github.com/nanopb/nanopb/tree/0.4.9.1)，纳入 `software/third_party/nanopb/` | zlib；保留原始 [LICENSE.txt](../software/third_party/nanopb/LICENSE.txt) 和 [版本/哈希](../software/third_party/nanopb/UPSTREAM.json) |
| ESP-IDF / NimBLE / NVS | [ESP-IDF 5.5.5](https://github.com/espressif/esp-idf/tree/v5.5.5) 及其组件 | Apache-2.0；组件保留各自上游声明 |
| mbedTLS | IDF 中的 3.6.6 | Apache-2.0 OR GPL-2.0-or-later；本项目采用 Apache-2.0 路径 |
| Gradle / Android Gradle Plugin | 8.11.1 / 8.10.1 | Apache-2.0 |
| Kotlin / Compose compiler plugin | 2.1.10 | Apache-2.0 |
| AndroidX Compose / Activity | BOM 2025.02.00 / Activity 1.10.1 | Apache-2.0 |
| protobuf Gradle plugin | [0.9.4](https://github.com/google/protobuf-gradle-plugin/tree/v0.9.4) | Apache-2.0 |
| protoc / protobuf Java runtime | [3.24.4](https://github.com/protocolbuffers/protobuf/tree/v24.4) | BSD-3-Clause |
| JUnit | 4.13.2；仅测试 | EPL-1.0 |

固件工具链版本见 [versions.json](../software/versions.json)，Android 依赖见 [版本目录](../software/android/gradle/libs.versions.toml)。Python 生成器依赖见 [requirements.txt](../software/protocol/host-tests/nanopb/requirements.txt)。Gradle Wrapper 的发行包版本和 SHA-256 固定在各工程的 `gradle-wrapper.properties` 中。

EPD0426A02 屏幕资料来源：[OSPTEK 官方仓库](https://gitee.com/osptek/4.26-epd-480x800-spi-ssd1677) 的 `versions/EPD0426A02`，参考提交 `332c6ef898f8b708e29191dd9e0a40b2216e3425`。规格书、波表和完整示例工程未在本仓库再分发，请从原厂获取并遵守相应许可。
