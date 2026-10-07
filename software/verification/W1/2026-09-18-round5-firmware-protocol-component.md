# W1 第五轮固件协议组件检查点

- 状态：`CHECKPOINTED`（nanopb 已接入固件源码组件图，ESP-IDF 镜像尚未构建）
- 开发窗口：2026-09-18 13:15–13:20（Asia/Shanghai，5 分钟）
- 基线提交：`bfb8782 build: add nanopb generated host conformance`
- 检查点：包含本报告的提交与 Git 标签 `checkpoint/round5-firmware-protocol-20260918`

## 本轮完成

- 新建 `firmware/components/mii_protocol` ESP-IDF component。
- 组件编译 nanopb 0.4.9.1 runtime，以及从规范生成的 `mii.pb.c`。
- `firmware/main` 显式依赖 `mii_protocol`，协议组件已进入固件组件图。
- 提交生成的 C/H 文件，使固件编译不隐式依赖开发机 Python 环境。
- 严格生成脚本会重新从 `protocol/mii.proto` 生成文件，并用 SHA-256 拒绝任何规范/提交产物漂移。
- 主机构建改为编译固件组件中提交的同一份生成源码，而非仅编译临时输出。

## 构建证据

| 检查 | 结果 |
|---|---|
| `tools\build_host.ps1 -Offline` | 退出码 0；独立 C++、固件 nanopb 源码、Kotlin 全部通过 |
| 重新生成一致性 | `NANOPB_COMMITTED_SOURCES_PASS` |
| 固件 nanopb 主机编译 | C11 runtime/生成源码及 C++17 测试编译、链接成功 |
| 三路径固定向量 | 均输出相同 31 字节 wire 数据 |
| 静态基线 | 108 项规则通过 |

## 验证边界

- 本轮验证了 CMake 组件声明、生成代码一致性和主机 native 编译，但没有可用 ESP-IDF 环境进行 configure/build。
- 因此尚未证明 Xtensa 编译、ESP-IDF 组件解析、最终链接或 Flash/RAM 增量。
- 没有构建 Android APK、烧录设备或进行实物板/手机验证。

## 准确续作顺序

1. 安装并锁定 ESP-IDF 5.3 或更新的具体版本。
2. 分别构建 Debug/Release，确认 `mii_protocol` 组件解析并记录镜像/分区余量。
3. 增加固件目标端协议自检，再进入 BLE/Wi-Fi 传输层开发。
4. 完成 W0 实物身份与关键电平复核后，才进入烧录和板级测量。
