# MII

MII 是一个低功耗、离线优先的智能墨水屏终端项目，包含硬件设计资料、ESP32 固件和 Android App。

目标功能包括手机编辑显示页面、温湿度与气压采集、GNSS 轨迹记录，以及 BLE / Wi-Fi 本地同步。手机负责编辑和数据管理，设备负责显示、采集与记录。

## 硬件

- ESP32-S3-MINI-1-N8：8 MB Flash，无 PSRAM
- 4.26 英寸 EPD0426A02 墨水屏：800 × 480
- BME280 环境传感器、E108-GN07IS GNSS 模块
- USB-C 充电与电池供电

## 目录

```text
hardware/              硬件说明与 SVG 接线参考
software/
  firmware/            ESP-IDF 固件
  android/             Kotlin / Jetpack Compose App
  protocol/            Protobuf 协议与测试向量
  tools/               构建、生成与静态检查脚本
  third_party/         固定版本依赖及原始许可证
  docs/                软件设计与依赖说明
  workflow/            开发与验证流程
  verification/        分阶段验证记录
docs/                  项目状态、资料来源与公开范围
```

## 开始使用

软件环境与构建入口见 [软件说明](software/README.md)，硬件资料见 [硬件说明](hardware/README.md)。

## 当前状态

项目仍在开发。固件已有协议、认证与 BLE 基础模块；Android App 已建立基础工程，完整编辑器、无线同步与整机功能仍待完善。已有主机测试与部分构建记录，实物驱动、功耗、无线互通和整机稳定性仍需验证。SVG 接线参考不是可直接投板的生产文件。

## 许可

软件采用 [MIT](LICENSES/MIT.txt)，硬件设计资料采用 [CERN-OHL-P-2.0](LICENSES/CERN-OHL-P-2.0.txt)。第三方代码保留各自许可证，范围见 [许可说明](LICENSE) 与 [第三方资料说明](docs/third-party.md)。
