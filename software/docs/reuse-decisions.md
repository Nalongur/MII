# MII 实现复用与轻量决策

规则起效：2026-10-07（用户明确长期要求）。调研是实现/构建前置，不是引入依赖
的自动批准。相同模块/固定版本可复用本记录；版本、需求、许可证或资源变化需刷新。

## 2026-10-07：当前认证、绑定与有界协议

| 实现 | 固定来源/许可证 | 兼容、依赖与资源 | 决策 |
|---|---|---|---|
| BLE / SC 数字比较 | [ESP-IDF v5.5.5 bleprph](https://github.com/espressif/esp-idf/blob/v5.5.5/examples/bluetooth/nimble/bleprph/main/main.c)，IDF 内含 NimBLE；Apache-2.0（本地 LICENSE 核对） | 已集成同版本 bt/controller/FreeRTOS，不需要 Arduino/第二蓝牙栈；沿用单连接和固定 RX/TX 队列，射频/栈水位待实板 | 采用现有 NimBLE SMP，不实现配对密码算法；示例 console/static passkey 不能替代设备显示与按键 |
| Protobuf | [nanopb 0.4.9.1](https://github.com/nanopb/nanopb/tree/0.4.9.1)，提交 `cad3c18ef15a663e30e3e43e3a752b66378adec1`；zlib 许可，本地 UPSTREAM/SHA-256 校验 | 既有三个 C runtime 源；固件 fixed max_size，唯一 proto；无需动态消息/反射运行时 | 扩展现有 schema 与 generator，不另写序列化器；测试用独立 stdlib 编码器仅用于固定向量 |
| HMAC-SHA-256 | IDF 内含 [mbedTLS 3.6.6](https://github.com/Mbed-TLS/mbedtls/blob/mbedtls-3.6.6/LICENSE)，Apache-2.0 OR GPL-2.0-or-later，采用 Apache-2.0 路径 | 同一 IDF crypto 配置，无第二 TLS 库；主机最小配置工作区峰值 232 B、每次 HMAC 零新增分配；S3 后端占用/耗时待测 | 复用 md/HMAC API，不写密码算法；一次检查分配、故障关闭、复用工作区 |
| 受保护持久化 | [IDF v5.5.5 NVS](https://github.com/espressif/esp-idf/blob/v5.5.5/components/nvs_flash/src/nvs_api.cpp)、nvs_sec_provider；Apache-2.0 | 专用 12 KiB 分区占原 pre-OTA 空隙，不移动 OTA/数据；需既有 HMAC_UP eFuse 配置；NVS 初始化堆/Flash 断电行为待测 | 复用 NVS 原子 blob 与保护派生，不做加密存储引擎；不自动生成/烧写保护密钥 |
| 所有者/生命周期/MAC gateway | MII 产品特有代码；不引入第三方框架 | 单任务、一候选、一挑战；固定 604 B wire 上限、512 B inner 上限；复用现有 64 项重放窗口 | 最小自研门控与绑定策略；成熟组件不提供 MII 的本地确认/单所有者/撤销 tombstone 语义，无需通用身份服务器或状态机框架 |

本记录是新增长期规则后的现状复核。round13/14 在规则下达前已复用官方组件并
验证；不追溯编造其 GitHub 调研时间。round15 后续构建引用本次 GitHub/本地源码核对。
没有更新上游依赖或修改 vendored runtime。现有 CRC/帧/队列保持既有证据。

## 2026-10-07：设备本地数字比较确认

沿用上表 v5.5.5 NimBLE `BLE_SM_IOACT_NUMCMP` / `ble_sm_inject_io`；同模块同版本
无需重复下载或搜索。SMP 密码运算、六位数字和对端确认由现有蓝牙栈承担。
仅最小自研 MII 特有的显示完成/fresh physical edge/连接代际/单次决定门控，
不用 GUI 或通用状态机库，不新增任务/队列/堆分配。实际 EPD/按键投递仍受 W0
与驱动验证门槛约束；没有显示回执时不能自动确认。

## 后续候选（尚未采用）

- BME280：[Bosch 官方 SensorAPI](https://github.com/boschsensortec/BME280_SensorAPI)。
  实施前固定提交并核对 BSD-3-Clause/单文件许可、I2C/延时薄适配和资源；不自写补偿公式。
- GNSS：[minmea](https://github.com/kosma/minmea) 与
  [IDF v5.5.5 UART/NMEA 示例](https://github.com/espressif/esp-idf/blob/v5.5.5/examples/peripherals/uart/nmea0183_parser/README.md)。
  实施前核对具体文件许可证、无堆/无浮点路径和 UART 任务代价，不简单称 minmea 全仓 MIT。
- EPD：[GxEPD2 SSD1677 参考](https://github.com/ZinggJM/GxEPD2/blob/master/src/gdeq/GxEPD2_426_GDEQ0426T82.cpp)。
  Arduino/Adafruit_GFX 依赖与 GPLv3 不适合直接引入当前工程；面板也未证明等同
  EPD0426A02。只作为候选调研入口，未复制代码，实施仍以原厂波形/本板证据为准。

## 2026-10-07：Android 最小工程（实施前核对）

| 来源/固定版本/许可证 | 兼容、依赖与资源决策 |
|---|---|
| 已验证 Gradle 8.11.1 wrapper/SHA-256，Apache-2.0；Kotlin 2.1.10，Apache-2.0 | 复用现有 wrapper 和 Kotlin，不升级 firmware/JVM 工具链；本机 JDK 21 执行，Java/Kotlin bytecode 17 |
| [AGP 8.10.1 官方兼容说明](https://developer.android.com/build/releases/agp-8-10-0-release-notes)，Apache-2.0 | 支持 API 36、要求 Gradle 8.11.1/JDK 17；compile/target 36、min 26、build-tools 36.0.0。缓存的 AGP 8.9.2 官方最高 API 35，故不为离线缓存压低/绕过 API 校验 |
| [官方 Compose compiler 配置](https://developer.android.com/jetpack/androidx/releases/compose-kotlin)，Kotlin compose plugin 2.1.10；BOM 2025.02.00、activity-compose 1.10.1；Apache-2.0 | 固定成熟版本，BOM 核对 Google Maven 原始 POM；只采用 UI/Material3 与必要 Activity，不引入 Hilt、整套导航/地图/样例工程；APK 大小在构建后记录 |
| [android/compose-samples Jetchat](https://github.com/android/compose-samples/blob/2292d2163b16aaca57e6d58a8eb573e374aa907d/Jetchat/app/build.gradle.kts)，Apache-2.0 | GitHub 路径最新提交 API 已核对；仅参考原生 Compose 构建/状态流，不复制整个聊天 App、调试签名 release 或其非必要依赖 |
| [protobuf-gradle-plugin 0.9.4](https://github.com/google/protobuf-gradle-plugin/tree/v0.9.4)，Apache-2.0；[protoc/runtime 3.24.4](https://github.com/protocolbuffers/protobuf/blob/v24.4/LICENSE)，BSD-3-Clause | 复用既有 generator 版本和唯一 mii.proto；Android 采用 javalite，无重复手写消息或反射 full runtime；复用现有八类向量检测源 |

最小 UI 只展示本地未绑定/空数据状态，不伪造设备值。BLE、Keystore、Room 与
同步后续逐个加入并先调研；本轮不启动设备/模拟器、扫描无线或部署。

## 检查点资源记录

每轮记录启用功能、实际镜像/静态段增量、任务/栈/队列/缓冲预算与未测项。
round13/14 的默认镜像裁剪了未调用认证函数，只证明基础配置构建，不能用于
评价完整认证功能大小。host 的 232 B 不能称 S3 实测工作区。
