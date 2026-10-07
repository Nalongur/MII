# W1 第十五轮：有界认证 wire 与同字节授权入口

- 日期：2026-10-07（Asia/Shanghai）；起点 `c5269c3`。
- 状态：`CHECKPOINTED_SOFTWARE_PREPARATION`；主机与目标编译通过。W0/W1/W8 未退出。
- 本地标签：`checkpoint/round15-auth-wire-20261007`，本报告与实现位于同一提交。
- ESP-IDF 5.5.5、mbedTLS 3.6.6、nanopb 0.4.9.1、JDK 21、Zig 0.16.0。
- V7/N8/8 MB/无 PSRAM；未连接、烧录或运行实板。

## 结果与复用

唯一 `mii.proto` 增加 BindingProposal/Accepted、SessionHello/Challenge/Proof/Ready、
AuthenticatedEnvelope、Failure 和带版本/关联 ID 的 AuthenticationPacket。
现有 Envelope 字段及原 31 B 固定向量不变；认证格式独立为 1，业务协议仍 1.0.0。
认证 inner 上限 512 B，包装仍在现有 604 B 帧载荷内。

严格拒绝 outer 重复字段、未知字段、非规范编码、错误版本/长度/缺关联 ID；
nanopb 流式重编码比较无需第二份 604 B scratch。MAC 先验证 exact inner bytes，
再解析受支持状态请求；设备 ID 从同一 immutable 二进制身份生成 32 位小写 hex，
不是由调用方提供。只有核对类型/版本/设备/非零 ID 后才更新重放窗口。

Windows/目标继续复用 nanopb、mbedTLS；没有新增库或密码算法。2026-10-07
GitHub 与固定 IDF 来源、许可、兼容和资源决策见 `docs/reuse-decisions.md`。
长期轻量/复用/额度/顺序开发规则已保存到软件仓库 `AGENTS.md` 与正式工作流。

## 验证证据

| 层级 | 命令/内容 | 结果 |
|---|---|---|
| 规范生成 | `tools/generate_nanopb.ps1` | 生成及提交源 SHA-256 一致；schema 改动后更新生成源 |
| 全主机回归 | `tools/build_host.ps1 -Offline` | 既有 C++/nanopb/Kotlin/帧/RX/TX/140 会话回归通过 |
| 最终静态校验 | `python tools/validate_baseline.py` | 205 项通过 |
| 当前 wire 定向回归/UBSan | `tools/test_auth_platform.ps1` | 597 wire、1,558 存储、1,033 mbedTLS 断言通过 |
| Kotlin 生成类型与 JVM HMAC | wrapper `check --offline` | 8 类 Python/nanopb/Kotlin packet 字节一致；四角色及 control HMAC 一致 |
| 目标编译 | `tools/build_firmware.ps1` | 最终 Debug/Release 都通过，未烧录 |
| 独立审查/Android运行/硬件与无线 | 未执行 | PENDING |

wire 断言包括每一种 packet 的所有截断长度、重复/未知外层字段、错误版本、
短 MAC、零关联 ID、最大 inner、未认证命令、错误 MAC 先于 protobuf 解码、
认证后的错误设备 ID 不推进重放、重复/乱序和旧连接事件拒绝。

## 轻量证据和未测边界

- 功能配置：BLE 服务仍关闭、业务 RX 隔离；认证未由 app_main 调用。
- 默认 Debug 镜像 229,120 B；Release 194,560 B；bootloader 20,832 B。
  认证函数被链接裁剪，不能称完整认证功能体积。
- `idf_size.py --format json`：默认 Debug `.data+.bss` 14,768 B，Release 14,280 B；
  与 round14 的 Flash map/静态数据 RAM 增量均 0 B，原因是功能关闭/裁剪。
- 主机 ABI：OwnerSession 1,008 B、OwnerRecord 136 B、generated auth packet 592 B，
  不作为 ESP ABI 或运行时实测。packet 编译有最大 640 B struct 约束。
- 目标编译器 `.su`：streaming canonical decoder Debug/Release 自身 96 B；
  gateway Debug 944 B、Release 848 B；MAC verifier Release 主体 704 B。
  原重编码 scratch decoder Release 为 672 B，改为 streaming 后减 576 B。
  以上是单函数静态栈估算，不是完整调用链或 FreeRTOS 栈水位。
- 本轮新增任务/队列为 0。既有候选协议任务栈 3,072 B（Xtensa StackType_t 为
  uint8_t）、RX 8 项/244 B 片段、TX producer/host 各 4 帧/616 B 帧；启用并集成
  前必须核对整个调用链和异常路径的栈预算，实板仍需测水位、堆和功耗。
- 日志位于 `build/host/round15-*.log`、尺寸 JSON；`.su` 在 profile mii_auth 组件目录。

## 下一步

1. 设备本地 Numeric Comparison 确认核心：有界窗口/显示完成、fresh button edge、
   代际、超时、断连和重复拒绝，沿用 NimBLE 官方 SMP，不使用静态 PIN/自动确认。
2. Android 最小 Kotlin/Compose 工程、Keystore、同一 proto、BLE MTU/CCCD/生命周期。
3. 可靠请求响应/业务 ACK/幂等、真实 dispatcher 与 queue_control_payload。
4. W0 实物身份/电平、显示/按键、保护配置及用户实际测试确认前不启用业务 RX
   或烧录；硬件依赖不得写成阶段 PASS。

最新额度：5 小时已用 50%，剩余 50%；持续 Goal active。
