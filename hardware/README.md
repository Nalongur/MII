# MII 硬件支持范围

固件面向 MII V7：ESP32-S3-MINI-1-N8（8 MB Flash，无 PSRAM）、EPD0426A02 墨水屏、BME280 和 E108-GN07IS GNSS 模块。

当前仓库未提供可制造的硬件发布包，无法据此直接投板或装配。PCB 原生工程、Gerber、装配文件和制造 BOM 尚未发布。

软件中的 GPIO 定义见 [board_v7.hpp](../software/firmware/components/board_v7/include/mii/board_v7.hpp)，构建配置见 [固件说明](../software/firmware/README.md)。实际接线、供电和器件版本仍需与所用板卡核对。

屏幕原厂资料来源见 [第三方资料说明](../docs/third-party.md)。

本目录项目自有硬件设计资料采用 [CERN-OHL-P-2.0](../LICENSES/CERN-OHL-P-2.0.txt)。来源地址：<https://github.com/Nalongur/MII/tree/main/hardware>。
