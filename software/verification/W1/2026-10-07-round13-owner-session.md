# W1 第十三轮：所有者绑定与认证会话核心

- 状态：`CHECKPOINTED_SOFTWARE_PREPARATION`，不构成 W0/W1/W8 退出。
- 日期：2026-10-07（Asia/Shanghai）；起点 `74bf473`，main 干净。
- 硬件目标：V7 / ESP32-S3-MINI-1-N8，8 MB，无 PSRAM；未连接或烧录。
- 固件版本保持 `0.1.0-dev.7`：此轮是尚未启用的核心准备，不改变启动行为。
- ESP-IDF 5.5.5、Windows Python 3.11、Zig 0.16.0、JDK 21。

## 实现

固定 `protocol/authentication_v1.md` 的威胁与集成边界。新增无堆分配的
`mii_auth` 核心：单所有者/单候选/单挑战；SC-only + 16 B 加密且认证链路门槛；
60 秒设备本地绑定窗；本地确认后才持久化；错误/不确定提交进入存储故障。
HMAC-SHA-256 按角色分离设备/手机证明、会话密钥和 ID，命令 MAC 先于重放更新。
断连清除临时凭据和会话；重启仅恢复已提交所有者；撤销保存递增代际 tombstone。
3 次失败后 60 秒冷却不因断连清除，挑战 30 秒、会话 15 分钟有界。

Windows 主机密码实现使用 CNG，固定向量独立由 Python hashlib 生成并在静态
校验中重新计算。nonce 源在测试中是可重复的测试桩，尚无设备随机数运行证据。
物理确认和链路证据均只能由可信适配层提供，不能取自手机字段。

## 命令和结果

| 证据层 | 命令/内容 | 实际结果 |
|---|---|---|
| 起点主机回归 | `tools/build_host.ps1 -Offline` | 191 静态项及既有 C++/nanopb/Kotlin/帧/RX/TX 通过 |
| 修改后主机回归 | `tools/build_host.ps1 -Offline` | 197 静态项、140 认证断言及既有回归通过 |
| 认证 UBSan | Zig `-O1 -fsanitize=undefined -fno-sanitize-recover=undefined` 编译同一测试 | 140 断言通过，无 UBSan 报告 |
| 目标组件编译 | `tools/build_firmware.ps1` | ESP-IDF Debug/Release 均编译 mii_auth 并生成镜像 |
| 镜像与分区 | 8 MB 草案 OTA 槽 `0x1e0000` | Debug 229,120 B；Release 194,560 B；bootloader 20,832 B |
| 差异空白校验 | `git diff --check` | 通过，仅既有 CRLF 转换提示 |
| 独立审查 | 未派其他代理 | 未完成，不称独立审查 PASS |
| Android / 实物 / 无线 | 未执行 | PENDING |

认证断言覆盖 RFC 4231 case 1、Python/CNG 四角色向量、未认证/短密钥/非 SC
拒绝、物理窗和确认过期、旧代际、断连候选清除、正常绑定/重启/解绑/重绑、
提交失败及“已持久化但回复丢失”、反射和旧挑战重放、会话过期、冷却、
错误 MAC、序号重复/有限乱序、密码端口失败、撤销失败、代际/时间溢出。

本地可重跑日志：`build/host/round13-host.log`、`round13-firmware.log`（忽略的
构建输出，报告保存结论与命令）。此轮组件未被 app_main 调用，最终链接会移除
未使用核心，因此镜像尺寸与 round12 一致；不是已启用认证固件。

## 尚未完成和准确续作

1. 实现 ESP-IDF mbedTLS/RNG port 与只读取既有保护密钥的 NVS port，验证固定
   blob 编码、CRC、版本/截断/故障恢复；禁止默认初始化自动生成 eFuse 密钥。
2. 唯一 proto 增加绑定/挑战/证明/MAC 包装，确认解析字段来自同一组 MAC 验证
   字节；当前核心的序号参数仍依赖可信 adapter，不能直接作为 wire dispatcher。
3. 建立 Android 最小工程与 Keystore 持久化，接入 SC 数字比较、按键/显示。
4. 可靠请求响应、业务 ACK 和幂等提交未完成，RX 隔离和默认关闭保持。
5. W0 实物确认、用户实际测试授权、目标内存/栈/电流及无线互操作仍需取得证据。

额度策略已保存到持续交接。最近检查：5 小时已用 24%，剩余 76%；目标继续 active。
