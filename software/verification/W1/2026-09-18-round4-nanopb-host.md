# W1 第四轮 nanopb 主机构建检查点

- 状态：`CHECKPOINTED`（双端生成代码主机测试通过，W1 总退出门槛尚未通过）
- 开发窗口：2026-09-18 10:05–10:19（Asia/Shanghai，14 分钟）
- 基线提交：`878ab51 build: generate Kotlin protobuf types`
- 检查点：包含本报告的提交与 Git 标签 `checkpoint/round4-nanopb-host-20260918`

## 本轮完成

- 从 nanopb 官方标签 `nanopb-0.4.9.1`、提交 `cad3c18e…` 引入最小 C runtime 与原始 Zlib 许可证。
- 用 `UPSTREAM.json` 固定来源提交及八个上游文件的 SHA-256，静态校验会检测漂移。
- 固定 nanopb 0.4.9.1、grpcio-tools 1.62.3、protobuf 4.25.6 生成器依赖。
- 新增严格生成脚本：任何未匹配的 `mii.options` 都会直接失败，生成物只进入被忽略的 `build/host/nanopb`。
- Zig 以 C11 编译 nanopb runtime 和生成的 `mii.pb.c`，再以 C++17 编译并链接主机测试。
- nanopb 生成类型完成固定向量编码、解码、内嵌 payload 解码，以及未来字段 127 的安全跳过。

## 构建证据

独立 C++、nanopb 与 Kotlin 三条路径均输出相同的 31 字节：

```text
0a02080110011a0d4d49492d544553542d303030312001280130013a020801
```

| 检查 | 结果 |
|---|---|
| `tools\build_host.ps1 -Offline` | 退出码 0；静态、独立 C++、nanopb、Kotlin 全部通过 |
| nanopb 严格生成 | `NANOPB_GENERATION_PASS`，两条容量 option 均匹配 |
| nanopb 编译 | runtime 与生成 C 代码通过 C11 编译，测试通过 C++17 编译和链接 |
| nanopb round-trip | 31 字节逐字节匹配，已知字段和 payload 正确解码 |
| nanopb 前向兼容 | 添加字段 127 后仍成功解码全部已知字段 |

开发中先后修正了 PowerShell 向 Python `-c` 传递双引号导致的语法错误，以及 C++ `auto` 无法从 nanopb 花括号初始化宏推断结构类型的问题；最终构建不包含这些失败。

## 验证边界

- 本轮是 Windows 主机上的 schema 生成与 native 编译，不是 ESP-IDF Xtensa 固件镜像。
- nanopb runtime 尚未声明为 ESP-IDF component，也未测量固件 Flash/RAM 增量。
- 没有构建 Android APK、烧录设备或进行实物板/手机验证。
- W0 实物身份记录仍为 `PENDING`。

## 准确续作顺序

1. 建立 firmware nanopb component，把规范生成步骤和 runtime 纳入 ESP-IDF 组件图。
2. 安装并锁定 ESP-IDF 版本，完成 Debug/Release 构建和镜像尺寸记录。
3. 锁定 Android SDK、AGP 与 Compose 兼容矩阵，建立最小 Android Debug 工程。
4. 完成 W0 实物身份与关键电平复核后，才进入烧录和板级测量。
