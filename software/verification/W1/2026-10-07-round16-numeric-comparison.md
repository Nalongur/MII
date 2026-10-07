# W1 第十六轮：本地数字比较确认门控

- 日期：2026-10-07；起点 `17f5bcc`。
- 状态：`CHECKPOINTED_SOFTWARE_PREPARATION`；W0/W1/W8 未退出。
- 本地标签：`checkpoint/round16-numeric-comparison-20261007`。

复用固定 ESP-IDF 5.5.5 NimBLE SMP，密码运算和六位数字仍由栈提供。
新增 MII 产品特有门控，不新增第三方库、任务、队列或动态分配。
设备物理窗口 60 秒，一次请求；比较最多 30 秒且不得延长原窗口。
显示真正完成后需要新的物理确认，预按住必须先释放；连接代际和 prompt ID
隔离旧回执、旧排队按键和重复事件。决定只交付一次，交付时过期改为拒绝。
本地取消可撤回尚未交给蓝牙栈的同意。

## 验证

| 层级 | 证据 | 结果 |
|---|---|---|
| 主机定向与 UBSan | `tools/test_auth_platform.ps1` | 56 门控、1,558 存储、1,033 mbedTLS、597 wire 断言通过 |
| 静态基线 | `python tools/validate_baseline.py` | 205 项通过 |
| ESP-IDF 目标编译 | `tools/build_firmware.ps1` | Debug/Release 均通过，未烧录 |
| 无线、显示、按键、独立审查 | 未执行 | PENDING |

本轮没有改动唯一 proto、既有 JVM/会话/帧实现；回归其 round15 证据，新增核心
已在主机 UBSan 和两种目标配置中真实编译。日志位于 `build/host/round16-*.log`。

## 资源与边界

- BLE 服务仍默认关闭，业务 RX 全部隔离。新门控未由 app_main/NimBLE 调用。
- 默认 Debug 镜像 229,120 B、Release 194,560 B、bootloader 20,832 B；
  Debug 静态数据 RAM 14,768 B、Release 14,280 B。相对 round15 Flash map 和
  静态数据 RAM 增量均 0 B，因为新功能未调用/被裁剪；不代表完整认证功能大小。
- 门控主机 ABI 88 B；目标 `.su` 单函数自身 Debug 最大 96 B、Release 最大 64 B。
  不等于完整调用链、S3 对象实测或运行时栈水位。新增任务/队列为 0，既有
  BLE 3,072 B 协议任务和固定 RX/TX 预算保持原记录，实际水位/堆/功耗待测。
- 接入时必须将返回 prompt ID 与当前 pending SMP 请求再次核对，随后由可信
  NimBLE host 上下文调用 `ble_sm_inject_io`。没有显示完成回执不能自动同意。
- 当前实验 BLE 仍有原 Just Works 配置；本轮不宣称 SC 数字比较配对已完成。

下一步建立 Android 最小工程与同一 proto/Keystore/BLE 基础；实际 EPD/按键桥接、
双端认证、请求响应/幂等与 W0 认可的设备测试完成前，业务 RX 不解锁。
