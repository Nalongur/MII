# W1 第十四轮：密码库与受保护 NVS 适配准备

- 状态：`CHECKPOINTED_SOFTWARE_PREPARATION`；W0/W1/W8 退出仍 PENDING。
- 日期：2026-10-07（Asia/Shanghai）；起点 `4f29fa6`。
- V7 / N8 / 8 MB / 无 PSRAM；未连接、烧录或启动实物。
- ESP-IDF 5.5.5，内含 mbedTLS 3.6.6；固件尚未启用认证功能，版本保持 dev.7。

## 实现与资源边界

`IdfAuthCrypto` 使用成熟 mbedTLS HMAC-SHA-256，仅初始化时检查并分配固定工作区；
每次证明复用，完成后把旧密钥 pads 替换为零密钥 pads。分配失败/部分分配失败
都释放并允许显式重新初始化；密码错误清除输出。RNG 仅在 BLE controller 已启用
时调用 `esp_fill_random`，避免把 controller 未启用时的输出当作合格随机数。

`IdfOwnerStore` 使用预先配置的 HMAC_UP 保护密钥 ID，只调用保护配置读取/派生 API，
不调用自动生成密钥或 eFuse 写入，不擦除任何分区。存储专用 NVS `mii_auth`
放在原对齐空隙 `0x12000`，大小 `0x3000`；原 NVS/OTA/数据区的偏移和容量不变。
这是未烧录的分区草案，W6 仍须实物容量/迁移验证。

确认 IDF `secure_init_partition` 对已初始化分区直接返回成功，因此适配层先
卸载自己独占的分区再用非空保护配置初始化，避免复用先前明文初始化。缺密钥、
错误保护配置、损坏数据和提交错误都关闭存储访问，不降级为明文绑定。

所有者记录固定 132 B：magic/格式/绑定标志/保留位/代际/112 B 凭据/CRC；不序列化
C++ struct padding。解绑记录保留代际且不含凭据。提交必须成功后重新打开只读句柄
并逐字节核对；任何不确定结果都锁定适配层直到显式重载。

## 验证证据

| 层级 | 命令/内容 | 结果 |
|---|---|---|
| 全主机回归 | `tools/build_host.ps1 -Offline` | 202 静态项、140 会话断言及既有 C++/nanopb/Kotlin/帧/RX/TX 通过 |
| 存储适配 UBSan | `tools/test_auth_platform.ps1` | 1,558 断言通过：全部 0～131 B 截断、1,056 个单 bit 错误、保护读取与初始化顺序、写入/提交/回读失败 |
| 真实密码库主机 UBSan | 同上，实际编译 IDF mbedTLS 源码 | 1,033 断言通过：RFC 4231、Python/CNG/mbedTLS 四角色输出一致、部分分配故障、1,000 次 HMAC 无新增分配 |
| 主机密码工作区 | 自定义 calloc/free 仪表 | 峰值 232 B，析构后零存活；不是 S3 目标实测值 |
| 最后修改后定向回归 | `tools/test_auth_platform.ps1` | RNG 拒绝时清除输出及上述测试通过 |
| 目标编译/镜像 | `tools/build_firmware.ps1`，最终源码再增量构建 | Debug/Release 组件编译和镜像生成通过；229,120 / 194,560 B，bootloader 20,832 B |
| 空白差异 | `git diff --check` | 通过 |
| 独立审查、Android运行、硬件/无线 | 未执行 | PENDING |

日志：`build/host/round14-host.log`、`round14-firmware.log`、
`round14-final-firmware.log`。固定 blob 向量由独立 Python zlib 生成并重新校验。
主机存储 API 和 RNG 使用明确的测试桩，没有操作 NVS/Flash/eFuse 或真实熵源。
目标构建编译适配层但 app_main 尚未调用；未使用函数被链接移除，不能声称适配层
已在目标运行或提供完整启用态链接证明。运行时堆/栈、电流、断电恢复未测。

## 精确下一步

1. 唯一 mii.proto 增加有界认证消息与 MAC 包装，C/nanopb/Kotlin 固定向量和拒绝
   路径一致；从被验证的同一原始 Envelope 解析序号与设备 ID，消除 adapter 参数错配。
2. 实现 SC 数字比较和带代际的本地确认事件核心；实际按键/EPD 验证等待 W0。
3. Android 最小 Kotlin/Compose 工程、Keystore 绑定记录、BLE 服务发现/MTU/CCCD。
4. 应用层请求响应/ACK/幂等和真实 dispatcher；认证门槛前业务 RX 不启用。

额度最新：5 小时已用 31%，剩余 69%；持续 Goal active。
