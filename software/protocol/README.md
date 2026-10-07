# MII 协议基线

状态：第十二轮 BLE 收发传输基线，协议版本 `1.0.0`、帧版本 `1`。当前文件用于固定字段号、nanopb 边界、编解码向量、会话内重放策略、传输帧格式与入口超时策略，不代表 W1 协议退出门槛已经通过。

## 文件

- `mii.proto`：平台无关的 Protobuf schema。
- `mii.options`：固件端 nanopb 静态容量上限。
- `test-vectors/device_status_request_v1.json`：C/C++ 与 Kotlin 必须共同复现的确定性 wire 向量。
- `framing_v1.md`：传输无关的固定帧头、CRC、分片、重同步与超时契约。
- `test-vectors/framed_device_status_request_v1.json`：包裹相同 Envelope 的确定性完整帧向量。
- `host-tests/cpp`：可由 Zig/Clang 构建的独立 C++ 最小 wire 编码检查。
- `host-tests/nanopb`：编译固件中的真实 C++ 编解码层，验证固定向量、边界错误、截断数据和未知字段。
- `host-tests/session`：直接编译固件会话状态实现，验证编号、重试、乱序、重放、淘汰、溢出和重置。
- `host-tests/frame`：直接编译固件帧实现，遍历全部 1–604 字节负载并验证分片、连续帧、CRC、重同步与超时。
- 同目录的 `frame_ingress_test.cpp`：验证连续帧交付、2 秒无进展超时、时钟回绕、坏帧恢复、业务拒绝与诊断计数。
- `host-tests/ble`：直接编译固件 BLE 收发状态机；RX 验证连接代际、244 字节写入上限、换连接清半帧、超时和断连拒绝，TX 验证 MTU 23/247 分片、四帧 FIFO、三次拥塞上限及生命周期清理。
- `host-tests/kotlin`：用锁定版 protoc 从规范生成 Java 消息类型，由 Kotlin/JVM 验证编码、解码与未知字段兼容性。

首次使用 nanopb 主机构建前安装锁定的 Python 生成器依赖：

```powershell
python -m pip install --user -r protocol\host-tests\nanopb\requirements.txt
```

首次运行允许 Gradle Wrapper 下载已锁定且带 SHA-256 校验的发行包和依赖：

```powershell
powershell -ExecutionPolicy Bypass -File tools\build_host.ps1
```

依赖进入本机缓存后，可用离线模式复验：

```powershell
powershell -ExecutionPolicy Bypass -File tools\build_host.ps1 -Offline
```

该入口真实运行独立 C++ wire 编码器、固件会话状态、流式帧和 BLE 收发会话测试，并从唯一规范生成 nanopb C 与 Kotlin/JVM 两端类型。nanopb 生成结果必须与 `firmware/components/mii_protocol/generated` 中提交的固件代码逐字节一致，主机构建随后编译固件真实编解码、会话状态、帧层和自检源码。`-Offline` 禁止 Gradle 联网，但要求上述 Python 依赖已经安装。

## v1 兼容规则

- 只允许用新字段号添加可选信息；不得复用字段号。
- 删除字段或改变既有字段语义必须提升主版本并提供迁移方案。
- `message_id` 由发送方分配；重试同一逻辑请求时保持不变，供接收方幂等处理。
- `sequence_number` 在发送方当前认证会话中单调递增，用于排序和重放检测。
- `sent_at_unix_ms` 是 UTC Unix 毫秒；没有可信时钟时允许为 `0`。
- `payload_type` 指明 `payload` 中独立序列化的具体消息。未知类型不得猜测解码。
- 当前 nanopb 上限为 32 字节设备 ID 和 512 字节控制消息载荷；大对象必须使用后续分块协议。

## 会话编号与重放策略

- 每次建立新的认证会话时，发送端和接收端状态都必须重置；当前状态只在 RAM 中。
- `message_id` 和 `sequence_number` 必须为非零值；发送计数器不允许回绕，耗尽后必须重建会话。
- 完全相同的 wire 重传可复用原 `message_id` 和 `sequence_number`；如果重新封装，应保持 `message_id` 不变并分配新的 `sequence_number`。两种形式在接收端都被判定为重复逻辑请求，不得再次执行副作用。
- 接收端使用 64 包滑动窗口，允许窗口内乱序，拒绝重复序号和超出窗口的过旧序号。
- 接收端只保留最近 64 个 `message_id`；被淘汰的 ID 不再受幂等缓存保护，必须由上层业务的持久化标识补足长时间幂等性。
- 此滑动窗口只是有状态的重放策略，不提供身份认证或消息完整性；后续传输层必须将它绑定到真实认证会话。

## 固定向量

`device_status_request_v1.json` 使用合成值，避免依赖真实设备身份或时钟。内部请求为：

```text
DeviceStatusRequest { include_runtime_counters: true }
=> 08 01
```

其 Envelope 逐字段编码如下：

| 字段 | wire bytes | 说明 |
| --- | --- | --- |
| `protocol_version` | `0a 02 08 01` | 长度 2 的子消息；仅 `major=1` 非默认 |
| `message_id` | `10 01` | 字段 2，varint 1 |
| `device_id` | `1a 0d 4d49492d544553542d30303031` | 13 字节 UTF-8 文本 `MII-TEST-0001` |
| `sequence_number` | `20 01` | 字段 4，varint 1 |
| `sent_at_unix_ms` | `28 01` | 字段 5，varint 1 |
| `payload_type` | `30 01` | `DEVICE_STATUS_REQUEST=1` |
| `payload` | `3a 02 08 01` | 长度 2 的已编码请求 |

连接后得到 31 字节：

```text
0a02080110011a0d4d49492d544553542d303030312001280130013a020801
```

## 流式帧

固定 Envelope 外层使用 12 字节帧头，详细格式见 [`framing_v1.md`](framing_v1.md)。当前测试向量的完整 43 字节帧为：

```text
4d4901001f00aef6c8ee7cac0a02080110011a0d4d49492d544553542d303030312001280130013a020801
```

其中帧头 CRC-16/CCITT-FALSE 为 `0xf6ae`，负载 CRC-32/IEEE 为 `0xac7ceec8`。CRC 只检测偶发传输损坏，不提供身份认证、加密或防重放能力。传输任务必须为未完成帧设置截止时间，并在处理完一个 ready 帧后释放缓冲，再继续消费同一批输入中的后续字节。

固件 `FrameIngress` 将无进展截止时间固定为 2,000 ms，使用调用方提供的 32 位单调毫秒计数，并支持正常回绕。业务处理器在 `consume()` 内同步执行，返回后不得保留 payload 指针；断连或新会话必须调用 `reset_session()`。

## 尚未验证

Kotlin/JVM、固件 C++ 编解码层、会话层、帧层、BLE 收发会话与两种 ESP-IDF 配置均已通过构建；另有一次临时启用 BLE 服务的 ESP-IDF 完整链接构建通过。Debug 镜像中也已链接启动自检。但尚未烧录，因此不宣称目标板已执行该自检。W1 完成前仍需：

1. 完成 W0 实物身份和关键电平复核后，进行首次受控烧录并保存启动日志。
2. 确认固定向量、会话状态和流式帧自检在 ESP32-S3 目标板实际运行通过。
3. 建立 Android 工程，在 Android 单元测试中复用 Kotlin 生成配置和固定向量。
4. 设计认证层，把 RAM 会话状态绑定到真实会话；认证完成前继续隔离 RX 命令，并禁止把当前 TX 骨架视为可信响应通道。
5. 在受控 V7 实物测试中验证广播、配对、MTU 交换、CCCD 加密门、真实通知拥塞、断连清理及 Android 互操作；主机和镜像构建不能替代 BLE/Wi-Fi 端到端验证。
