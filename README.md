# MII

MII 是一个低功耗、离线优先的智能墨水屏终端项目。本仓库提供 ESP32 固件、Android App 源码、协议和构建工具。

目标功能包括手机编辑显示页面、温湿度与气压采集、GNSS 轨迹记录，以及 BLE / Wi-Fi 本地同步。手机负责编辑和数据管理，设备负责显示、采集与记录。

## 硬件

- ESP32-S3-MINI-1-N8：8 MB Flash，无 PSRAM
- 4.26 英寸 EPD0426A02 墨水屏：800 × 480
- BME280 环境传感器、E108-GN07IS GNSS 模块
- USB-C 充电与电池供电

## 目录

```text
hardware/              硬件支持范围说明
software/
  firmware/            ESP-IDF 固件
  android/             Kotlin / Jetpack Compose App
  protocol/            Protobuf 协议与测试向量
  tools/               构建、生成与静态检查脚本
  third_party/         固定版本依赖及原始许可证
docs/                  第三方依赖与许可说明
```

## 开始使用

克隆仓库后，按 [软件说明](software/README.md) 配置环境、运行测试和构建。硬件支持范围见 [硬件说明](hardware/README.md)。

## 当前状态

项目仍在开发，尚无可直接安装使用的正式版本。固件包含协议、认证与 BLE 基础模块，BLE 服务默认关闭；Android App 为基础工程，完整编辑器、无线同步与整机功能尚未完成。当前源码不能作为已完成实物验证的整机固件。仓库暂未提供 PCB 原生工程、Gerber、装配文件或制造 BOM。

## 许可

软件采用 [MIT](LICENSES/MIT.txt)，硬件设计资料采用 [CERN-OHL-P-2.0](LICENSES/CERN-OHL-P-2.0.txt)。第三方代码保留各自许可证，范围见 [许可说明](LICENSE) 与 [第三方资料说明](docs/third-party.md)。
