# MII V7 六层 PCB 详细接线与布局说明 v1.0

日期：2026-09-01  
适用原理图：V7，Schematic UUID `9a81bf47b5d6dd21`，Page UUID `a1746ebef106fb83`  
用途：把当前已通过网络复核的原理图落实为约 `52 mm x 62 mm`、板厚 `0.6 mm`、六层、单面装配 PCB。

## 1. 当前设计目标与冻结条件

- 4.26 英寸 OSPTEK `EPD0426A02`，800 x 480，SSD1677，24 Pin FPC。
- 屏幕位于产品正面，屏幕 FPC 金手指位于显示正面；J2 放在 62 mm 底边附近，但 Pin1 左右方向仍必须用屏幕实物和连接器图确认。
- 主控为 `ESP32-S3-MINI-1-N8`。
- GNSS 为 `E108-GN07IS`，不保留星历，`VBCKP` 悬空。
- GNSS 天线已经改为直接焊在主 PCB 上的 `BWGNSCNX18-18W2 / C2826453`，18 x 18 x 2 mm 单频 L1 无源陶瓷天线，不再使用 U.FL 和同轴线。
- 电池为 1S 1200 mAh、约 3 x 50 x 60 mm、保护板已集成、红黑两线。
- 电源接口为 `TYPE-C-31-M-12 / C165948`，USB-C 同时用于供电、充电和 ESP32 USB 通信。
- SW1 为电源键，SW2、SW3 为功能键，统一 `SKTDLDE010`。
- LED1 为红色充电灯，LED2 为绿色开机灯。
- U8 为 ZY-BME280 八焊盘模块，母板封装按 14.86 x 11.50 mm、2.54 mm 纵向节距估算，量产前必须按实物复测。
- 所有元件只放在同一面；底面可以布线和铺铜，但不放器件。
- 框内可用高度约 4.3 到 4.5 mm，电池、PCB 和天线不做垂直堆叠。

当前 V7 网络检查结果：94 个 PCB/BOM相关元件，非 NC 悬空引脚 0，重复位号 0，引脚多网络冲突 0；DRC 为 0 错误、3 条预留单端网络警告。

## 2. 整机电源和数据总路径

### 2.1 电源路径

```text
USB-C VBUS
    |
    v
USB_5V -> U3 BQ24074 -> SYS_OUT -> U5 TPS63021 -> 3V3_MAIN
                     |                         |
                     |                         +-> ESP32、EPD、BME280、U6
                     +-> 电池 BAT              |
                                               +-> U6 TPS22919 -> 3V3_GNSS -> U7
```

- USB 插入时，U3 完成充电和 PowerPath 管理。
- 电池工作时，U3 仍从 BAT 生成 SYS_OUT。
- U4 STM6601、Q2 和 U5 EN 组成软关机/长按复位逻辑。
- 关机后 U3 仍可充电；U5 关闭后 3V3_MAIN 断电。

### 2.2 主要数据路径

```text
USB-C D+/D- -> U2 ESD -> R5/R6 -> ESP32 GPIO20/GPIO19
ESP32 SPI -> J2 -> SSD1677 墨水屏
ESP32 UART -> U7 E108-GN07IS
ESP32 I2C -> U8 BME280 模块
U7 RF_IN -> C34 -> J3 板载陶瓷 GNSS 天线
```

## 3. 六层板推荐叠层功能

最终线宽必须在选定板厂的 `0.6 mm 六层阻抗叠层`确定后计算。不能直接沿用旧版四层板或经验线宽。

| 层 | 推荐用途 | 强制要求 |
|---|---|---|
| L1 Top | 全部器件、USB 差分、GNSS 50 ohm、短高速/高电流回路 | USB、GNSS 尽量全程留在 L1；器件只放此面 |
| L2 | 完整 GND 参考平面 | 除天线厂家明确要求的区域外不分割、不走信号 |
| L3 | 电源分配 | BAT、USB_5V、SYS_OUT、3V3_MAIN、3V3_GNSS 分区；不要把电源岛切断 L1 信号的 L2 回流 |
| L4 | 低速信号、控制、辅助电源 | 只承担低速跨区线；使用 L5 作为回流参考 |
| L5 | 第二完整 GND 平面 | 尽量连续，和 L2 用地过孔阵列连接 |
| L6 Bottom | 少量低速信号、GND 铜、测试焊盘 | 不放器件；避免形成长开口或天线状铜皮 |

六层板的核心不是“能走更多线”，而是给 L1 USB、GNSS、开关电源和 EPD 提供稳定的近距离地参考。L2 和 L5 不要切成多个孤立地岛。噪声隔离主要靠器件分区和回流路径控制，而不是把 GND 平面割开。

### 3.1 需要板厂确认后才能冻结的参数

1. L1 到 L2 的介质厚度、铜厚和成品铜厚。
2. L4 到 L5 的介质厚度。
3. L1 USB 90 ohm 差分线宽、间距、阻焊模型。
4. L1 GNSS 50 ohm 单端线宽；优先采用带地共面波导时，还要确定线到同层地铜间距。
5. 最小孔径、焊盘、孔到铜距离，以及 0.6 mm 六层板是否支持所选 USB 固定槽孔。

在这些数据确定前，可以摆位和规划拓扑，但不要把 USB/GNSS 线宽写死为某个数值。

## 4. PCB 分区和摆放顺序

### 4.1 必须先锁定的机械件

按以下顺序摆放并锁定，随后其他元件围绕它们展开：

1. PCB 外形、圆角、卡扣、螺柱和电池禁布区。
2. J2 FPC：位于 62 mm 底边，按屏幕 FPC 自然方向确定开口和 Pin1。
3. J1 USB-C：位于一个 52 mm 侧边，外壳开口、PCB Edge、槽孔完全按规格图。
4. SW1、SW2、SW3：位于另一侧边，推杆中心和外壳按键柱同轴。
5. J3 18 x 18 mm GNSS 陶瓷天线：白色陶瓷面朝塑料背板/天空方向，上方不放电池或金属。
6. U1 ESP32 模块：PCB 天线端伸到板边，优先让天线部分伸出母板边缘。
7. 电池红黑线焊盘和拉力释放结构。

### 4.2 推荐功能区

| 区域 | 元件 | 主要目标 |
|---|---|---|
| USB/充电区 | J1、U2、D4、R3、R4、U3、C14-C16、R7-R12、LED1、D5 | USB 入口保护和充电回路短、宽、靠边 |
| 软开关区 | U4、Q2、C17-C19、R13-R18、R32 | 靠近 U3/U5，同时方便连到 SW1、SW3 和 ESP |
| 3.3 V 开关电源区 | U5、L2、C20-C25、C22 | 最小化高 di/dt 回路，远离两个天线和 BME280 |
| ESP 区 | U1、C26-C28、R5、R6、R18、R19、R30、R31 | 天线朝板边；电源去耦和 USB 串联电阻靠模块 |
| GNSS 区 | J3、U7、C34、U6、C30-C33、R27 | 天线远离噪声源；RF 馈线最短、50 ohm、无支线 |
| EPD 高压区 | J2、L1、Q1、D1-D3、R1、R2、C1-C13 | 紧贴 FPC；高压泵和 SPI 不穿越 GNSS 区 |
| 环境传感区 | U8、R21-R26 | 靠壳体通气孔，远离 U3/U5/L1/L2、LED 和电池热源 |
| 按键/指示区 | SW1-SW3、D6、D7、LED1、LED2 | 机械位置优先，避免金属按键壳靠近 ESP/GNSS 天线 |

## 5. 必须相互贴近的元件组

表中距离是布局目标，不是封装庭院的替代。优先保证回路面积和回流连续性。

| 优先级 | 主器件 | 必须贴近的器件 | 建议关系 |
|---|---|---|---|
| A | J1 | U2、D4、R3、R4 | U2 和 D4 应是信号/电源进入 PCB 后最先遇到的保护器件；尽量在 J1 后 1-5 mm 内 |
| A | U3 BQ24074 | C14、C15、C16 | 各电容尽量在对应 IN/BAT/OUT 引脚 1-2 mm 内；电容地端直接进入 U3 地区 |
| A | U3 | R7、R8、R9 | ISET、ILIM、TS 都是模拟设定脚，电阻靠 U3，走线短且远离 L1/L2 开关节点 |
| A | U5 TPS63021 | L2、C20、C21、C22、C23、C24、C25 | 必须形成紧凑功率岛；L1/L2 开关节点不得拉长或用过孔换层 |
| A | J2 | L1、Q1、D1-D3、C1-C13、R1、R2 | 全部位于 J2 后方同一小区；高压泵节点短小，不跨越其他功能区 |
| A | U7 | C34、J3 | `RF_IN -> C34 -> J3` 尽量同一直线、同一层、无过孔、无测试点、无分叉 |
| A | U6 | C30、C31、R27 | 输入、输出去耦和 ON 下拉紧贴负载开关 |
| A | U7 | C32、C33 | 10 uF + 0.1 uF 紧靠 U7 VCC/GND，引脚到电容再到地的环路最短 |
| A | U1 | C26、C27 | 10 uF + 0.1 uF 紧靠 U1 3V3 和地焊盘 |
| A | U1 EN | R18、C28、U4 RST# 支线 | EN 节点短，避开开关节点和按键长线 |
| B | U4 | C17、C18、C19 | VCC、VREF、CSRD 各自电容靠对应引脚，不能共用一条细长地线 |
| B | Q2 | R13、R14、R15 | 三颗电阻围绕 Q2，`STM_EN_N -> Q2 -> TPS_EN` 不跨区 |
| B | LED1 | R10 | R10 靠 LED1 阳极；LED1 位置服从红色导光孔 |
| B | LED2 | R20 | R20 靠 LED2 阳极；R19 靠 LED_N/GPIO48 支线 |
| B | U8 | R21、R22、R23-R26 | I2C 上拉和供电/地址选择电阻在 U8 周围，丝印清楚标注 DNP |
| B | U1 ADC | R28、R29、C29 | 三者围绕 GPIO4；C29 最靠 ADC 引脚，BAT 高阻支线不靠 L1/L2 |
| B | SW3 | D6、D7、R31 | 两只二极管分开朝向 GPIO1 和 U4 SR；不要出现长公共 KEY2_NODE 支线 |

去耦电容的原则是“电源引脚先到电容，再进入大电源铜；电容地端立即到完整地平面”。不要把去耦电容放得看起来靠近，但实际用细长线绕一圈才接到引脚。

## 6. 逐接口、逐芯片接线

### 6.1 J1 USB-C：TYPE-C-31-M-12

| J1 引脚 | 网络 | 去向/说明 |
|---|---|---|
| A1/B12、B1/A12 | GND | 接连续地平面 |
| 固定壳脚 1-4 | GND | 接大面积地铜，并在壳脚周围布置地过孔 |
| A4/B9、B4/A9 | USB_5V | 接 D4 Pin1、C14 Pin1、U3 Pin13 |
| A5 | USB_CC1 | 接 R3 Pin1 和 U2 Pin4 |
| B5 | USB_CC2 | 接 R4 Pin1 和 U2 Pin5 |
| A6、B6 | USB_DP_CONN | 正反面 D+ 在连接器附近并接，接 U2 Pin1 和 R5 Pin1 |
| A7、B7 | USB_DM_CONN | 正反面 D- 在连接器附近并接，接 U2 Pin2 和 R6 Pin1 |
| A8、B8 | NC | 不走长支线、不放测试点 |

布局注意：

- `PCB Edge`、0.50 mm 信号脚、固定槽孔和定位结构严格使用 C165948 专用封装。
- U2 必须靠 J1，保护线从 J1 先到 U2，再进入板内。
- R5/R6 是 ESP 端串联电阻，应靠 U1，不要靠 J1。
- USB D+/D- 在 L1 以 90 ohm 差分布线，成对换层，换层时在信号过孔旁加 GND 回流过孔。
- 两线等长以几何对称为主；USB Full Speed 不需要为了极端等长而蛇形绕线。

### 6.2 U2 TPD4E05U06 ESD 阵列

| Pin | 网络 | 说明 |
|---:|---|---|
| 1 | USB_DP_CONN | D+ 对地 ESD |
| 2 | USB_DM_CONN | D- 对地 ESD |
| 3、8 | GND | 最短、最宽接 L2 地；每个地端附近至少一个地过孔 |
| 4 | USB_CC1 | CC1 对地 ESD |
| 5 | USB_CC2 | CC2 对地 ESD |
| 6、7、9、10 | NC | 保持悬空 |

### 6.3 U3 BQ24074 充电与 PowerPath

| Pin | 网络 | 连接 |
|---:|---|---|
| 1 TS | BQ_TS | R9 10 kOhm 到 GND；当前两线电池不用 NTC |
| 2、3 BAT | BAT | 电池红线、C15 Pin1、R28 Pin1 |
| 4 CE# | GND | 始终允许充电 |
| 5 EN2 | GND | 与 EN1 组成 USB500 模式 |
| 6 EN1 | SYS_OUT | 高电平，固定约 500 mA 输入模式 |
| 7 PGOOD# | PGOOD | R12 10 kOhm 上拉 3V3_MAIN，接 U1 GPIO47 |
| 8 VSS | GND | 直接接地 |
| 9 CHG# | CHG_N | LED1 阴极、D5 阴极 |
| 10、11 OUT | SYS_OUT | C16、U4 VCC、U5 VIN/VINA、R10、R14 |
| 12 ILIM | BQ_ILIM | R8 1.5 kOhm 到 GND |
| 13 IN | USB_5V | J1 VBUS、D4、C14 |
| 14 TMR | NC | 使用默认安全计时 |
| 15 ITERM | NC | 使用默认终止比例 |
| 16 ISET | BQ_ISET | R7 2.21 kOhm 到 GND，目标约 400 mA |
| 17 EP | GND | 焊接到地焊盘；不能只依赖 EP，Pin8 也必须接地 |

TI 布局要求落实方式：

- C14、C16 首先紧贴 IN/OUT 和 EP/VSS；C15 紧贴 BAT。
- USB_5V、BAT、SYS_OUT 使用宽铜皮或粗线，避免压降。
- R7/R8/R9 的小信号地回路在 U3 附近汇入地，不与充放电大电流路径串联。
- EP 下方按封装推荐放热过孔；若过孔位于锡膏区，应使用塞孔/盖孔工艺，防止吸锡。

### 6.4 电池 BT1

- 红线接 BAT，黑线接 GND。
- 当前原理图中 BT1 `AddIntoPcb=false`，意味着直接导入 PCB 时不会生成电池焊盘。这不是可以忽略的状态。
- 进入正式 PCB 前必须增加实际可制造的 `BAT+`、`GND` 焊线结构：建议使用两个大焊盘或通孔焊盘，并增加独立拉力释放孔/扎带孔。
- 不要只用 0603 大小焊盘焊电池线；0.6 mm PCB 较软，焊点必须有机械减载。
- 电池已经带保护板，不再串联第二套保护板；R9 仍用于 BQ24074 TS 合法偏置。

### 6.5 充电状态灯和 PGOOD

```text
SYS_OUT -> R10 3.3 kOhm -> CHG_LED_A -> LED1 Pin2(A)
LED1 Pin1(K) -> CHG_N -> U3 Pin9

3V3_MAIN -> R11 100 kOhm -> CHG_SENSE -> U1 GPIO13
CHG_SENSE -> D5 Pin2(A)
D5 Pin1(K) -> CHG_N

3V3_MAIN -> R12 10 kOhm -> PGOOD -> U1 GPIO47
```

U3 CHG# 拉低时红灯亮。LED1、R10 放在导光孔和 U3 之间；CHG_SENSE 支路与大电流路径分开。

### 6.6 U4 STM6601、Q2 和电源键

| U4 Pin | 网络 | 连接 |
|---:|---|---|
| 1 VCC | SYS_OUT | C17 0.1 uF 到 GND |
| 2 SR# | PWR_SR | D7 Pin2/A |
| 3 VREF | STM_VREF | 只接 C18 1 uF 到 GND |
| 4 PSHOLD | PSHOLD | U1 GPIO21，R32 100 kOhm 到 GND |
| 5 CSRD | STM_CSRD | C19 0.47 uF 到 GND |
| 6 PB# | PWR_PB | SW1 Pin1，按下接地 |
| 7 VCC#LO | NC | 悬空 |
| 8 PB#OUT | PWR_PBOUT | R16 10 kOhm 上拉，接 U1 GPIO2 |
| 9 EN(EN#) | STM_EN_N | 经 R13 100 kOhm 到 Q2 基极 |
| 10 RST# | ESP_EN | U1 EN、R18 10 kOhm、C28 1 uF |
| 11 INT# | PWR_INT | R17 10 kOhm 上拉，接 U1 GPIO14 |
| 12 GND | GND | 接完整地平面 |

Q2 `MMBT3904`：Pin1/Base 接 Q2_BASE，Pin2/Emitter 接 GND，Pin3/Collector 接 TPS_EN。R14 100 kOhm 从 SYS_OUT 上拉 TPS_EN，R15 1 MOhm 从 Q2_BASE 下拉 GND。

### 6.7 U5 TPS63021 固定 3.3 V

| Pin | 网络 | 连接 |
|---:|---|---|
| 1 VINA | SYS_OUT | C22 0.1 uF |
| 2 GND | GND | 控制地 |
| 3 FB | 3V3_MAIN | 固定输出版本反馈直接接输出 |
| 4、5 VOUT | 3V3_MAIN | C23、C24、C25 各 22 uF |
| 6、7 L2 | TPS_L2 | 接电感 L2 Pin2 |
| 8、9 L1 | TPS_L1 | 接电感 L2 Pin1 |
| 10、11 VIN | SYS_OUT | C20、C21 各 10 uF |
| 12 EN | TPS_EN | Q2 集电极和 R14 |
| 13 PS/SYNC | GND | 使用省电模式 |
| 14 PG | NC | 当前悬空 |
| 15 PGND | GND | 功率地和散热焊盘 |

布局必须形成四个最小回路：

1. `VIN -> U5 -> PGND -> C20/C21 -> VIN`
2. `VOUT -> C23/C24/C25 -> PGND -> U5`
3. `U5 L1 pins -> L2 -> U5 L2 pins`
4. `VINA -> C22 -> GND`

L2 与 U5 同层直连。TPS_L1、TPS_L2 是高 dv/dt 开关节点，只铺完成连接所需的最小铜，不铺成长条，不跨层，不靠近 GNSS、BME280、USB D+/D- 或 ESP 天线。

### 6.8 U1 ESP32-S3-MINI-1-N8

| U1 Pin/GPIO | 网络 | 用途 |
|---|---|---|
| Pin3 3V3 | 3V3_MAIN | C26 10 uF + C27 0.1 uF |
| GPIO0 | FUNC1_BOOT | SW2，R30 10 kOhm 上拉 |
| GPIO1 | FUNC2_GPIO | SW3 经 D6，R31 10 kOhm 上拉 |
| GPIO2 | PWR_PBOUT | U4 电源键短按状态 |
| GPIO4 | BAT_ADC | R28/R29 分压，C29 0.1 uF |
| GPIO5 | BME_SDA | U8 Pin4，R21 4.7 kOhm 上拉 |
| GPIO6 | BME_SCL | U8 Pin3，R22 4.7 kOhm 上拉 |
| GPIO7 | EPD_BUSY | J2 Pin9 |
| GPIO8 | EPD_RST | J2 Pin10 |
| GPIO9 | EPD_DC | J2 Pin11 |
| GPIO10 | EPD_CS | J2 Pin12 |
| GPIO11 | EPD_MOSI | J2 Pin14 |
| GPIO12 | EPD_SCLK | J2 Pin13 |
| GPIO13 | CHG_SENSE | 充电状态读取 |
| GPIO14 | PWR_INT | U4 INT# |
| GPIO15 | GNSS_EN | U6 ON，R27 100 kOhm 下拉 |
| GPIO16 | GNSS_1PPS | U7 Pin3 |
| GPIO17 | GNSS_RXD | ESP TX 到 U7 RXD Pin21 |
| GPIO18 | GNSS_TXD | ESP RX 从 U7 TXD Pin20 |
| GPIO19 | USB_DM_MCU | R6 后的 USB D- |
| GPIO20 | USB_DP_MCU | R5 后的 USB D+ |
| GPIO21 | PSHOLD | U4 Pin4，R32 100 kOhm 下拉 |
| GPIO47 | PGOOD | U3 输入电源有效状态 |
| GPIO48 | LED_N | 绿色开机灯低端控制 |
| TXD0 Pin39 | U0TXD | 预留调试串口输出 |
| RXD0 Pin40 | U0RXD | 预留调试串口输入 |
| EN Pin45 | ESP_EN | U4 RST#、R18、C28 |
| 所有 GND/中央焊盘 | GND | 接完整地平面并用多个地过孔连接 L2/L5 |

ESP 模块天线区域：

- 模块 PCB 天线端优先伸出母板边缘。
- 如果不能伸出，天线下方和前方所有铜层、走线、器件必须禁布；不要只清 L1。
- 天线附近外壳净空建议至少 15 mm，不能有电池、金属按键壳、螺柱或 FPC 横跨。
- 模块天线和 GNSS 陶瓷天线尽量放在不同边或对角，避免互相遮挡。

### 6.9 按键和绿色开机灯

| 器件 | 接线 |
|---|---|
| SW1 | Pin1 PWR_PB，Pin2 GND，Pin3/4 NC |
| SW2 | Pin1 FUNC1_BOOT，Pin2 GND，Pin3/4 NC |
| SW3 | Pin1 KEY2_NODE，Pin2 GND，Pin3/4 NC |
| D6 | Pin1/K KEY2_NODE，Pin2/A FUNC2_GPIO |
| D7 | Pin1/K KEY2_NODE，Pin2/A PWR_SR |
| LED2 | Pin1/K LED_N，Pin2/A PWR_LED_A |
| R20 | 3V3_MAIN 到 PWR_LED_A，4.7 kOhm |
| R19 | 3V3_MAIN 到 LED_N，100 kOhm |

GPIO48 输出低时 LED2 亮；输出高或高阻时熄灭。LED2 已在当前 V7 中重新接线并通过网络复核。

### 6.10 U6、U7 和板载 GNSS 天线

U6 TPS22919：

| Pin | 网络 | 说明 |
|---:|---|---|
| 1 IN | 3V3_MAIN | C30 1 uF 到 GND |
| 2 GND | GND | 就近地过孔 |
| 3 ON | GNSS_EN | U1 GPIO15，R27 100 kOhm 下拉 |
| 4 NC | NC | 悬空 |
| 5 QOD | 3V3_GNSS | 与 VOUT 相连，关断快速放电 |
| 6 VOUT | 3V3_GNSS | C31 1 uF，到 U7 电源入口 |

U7 E108-GN07IS：

| Pin | 网络 | 说明 |
|---:|---|---|
| 3 1PPS | GNSS_1PPS | U1 GPIO16 |
| 10、12、13、24 | GND | 接完整地平面，模块周边布地过孔 |
| 11 RF_IN | GNSS_RF | 接 C34 Pin1 |
| 20 TXD | GNSS_TXD | 接 U1 GPIO18 |
| 21 RXD | GNSS_RXD | 接 U1 GPIO17 |
| 23 VCC | 3V3_GNSS | C32 10 uF + C33 0.1 uF |
| 8 RESET_N | NC | 当前不使用 |
| 9 VCC_RF | NC | 无源天线不供电 |
| 22 VBCKP | NC | 不保留星历 |
| 其余 NC | NC | 不走支线 |

RF 链路：

```text
U7 Pin11 RF_IN / GNSS_RF
        -> C34 Pin1
        -> C34 56 pF
        -> C34 Pin2 / GNSS_ANT
        -> J3 Pin1 / BWGNSCNX18-18W2
```

板载陶瓷天线的关键注意事项：

- 这是一只 GNSS 陶瓷贴片天线，不应照搬 ESP PCB 天线的“所有层全部禁铜”规则。大多数 GNSS patch 需要规定尺寸的地平面作为反射面。
- 当前 J3 原理图只有一个 RF 引脚，无法表达天线底部接地金属、中心馈点和机械焊接面。正式 PCB 前必须拿到 `BWGNSCNX18-18W2` 的推荐落地图，核对现有 `ANT-TH_L18.0-W18.0_C9900026472` 封装。
- 在没有原厂落地图前，不得凭封装名字判断“底部铺地”或“底部禁铜”。如果原厂图确认底面为接地面，应将其连接到大面积顶层地，并用密集地过孔连接 L2/L5。
- 陶瓷白色辐射面朝塑料背板/天空；其正上方不能有电池、金属背板、屏蔽罩、FPC、螺钉或导电泡棉。
- 尽量让天线拥有最大、连续、对称的地平面；不要把它塞进被电池和板边切碎的小角落后仍期待标称增益。
- U5/L2 和 EPD L1/Q1/D1-D3 离天线越远越好，目标至少 15 mm，空间允许时取更大距离。
- GNSS_RF/GNSS_ANT 走 L1、50 ohm、无过孔、无测试点、无分叉；两侧可用接地共面铜和地过孔围栏，但具体间距由阻抗计算决定。
- C34 和天线馈点周围禁止铺与 RF 无关的铜皮或走其他信号。
- 整机装配后必须在真实外壳、电池和屏幕状态下测冷启动时间、C/N0、定位稳定性；裸板能定位不代表整机满足要求。

### 6.11 J2 EPD0426A02 24 Pin FPC

| J2 Pin | 网络 | 接线/器件 |
|---:|---|---|
| 1 | NC | 悬空 |
| 2 GDR | GDR | Q1 Pin1/G，R1 10 kOhm 到 GND |
| 3 RESE | RESE | Q1 Pin2/S，R2 2.2 ohm 到 GND |
| 4 | NC | 悬空 |
| 5 VSH2 | VSH2 | C2 4.7 uF/25 V 到 GND |
| 6、7 | GND | 地平面 |
| 8 BS1 | GND | 选择 4 线 SPI |
| 9 BUSY_N | EPD_BUSY | U1 GPIO7 |
| 10 RST_N | EPD_RST | U1 GPIO8 |
| 11 D/C | EPD_DC | U1 GPIO9 |
| 12 CSB | EPD_CS | U1 GPIO10 |
| 13 SCL | EPD_SCLK | U1 GPIO12 |
| 14 SDA | EPD_MOSI | U1 GPIO11 |
| 15 VDDIO | 3V3_MAIN | 逻辑电源 |
| 16 VCC | 3V3_MAIN | C6 22 uF + C7 0.1 uF |
| 17 VSS | GND | 地平面 |
| 18 VDD | EPD_VDD | 只接 C8 1 uF 到 GND，不接 3V3_MAIN |
| 19 VPP | NC | 悬空 |
| 20 VSH1 | VSH1 | C9 4.7 uF/25 V 到 GND |
| 21 VGH | VGH | D3 Pin1/K，C5 4.7 uF、C10 1 uF 到 GND |
| 22 VSL | VSL | C11 4.7 uF/25 V 到 GND |
| 23 VGL | VGL | D1 Pin1/K，C1 4.7 uF、C12 1 uF 到 GND |
| 24 VCOM | VCOM | C13 1 uF 到 GND |
| 25、26 固定脚 | GND | 连接器金属保持脚接地 |

EPD 高压泵：

```text
3V3_MAIN -> L1 10 uH -> EPD_SW
Q1: G=GDR, S=RESE, D=EPD_SW
D3: A=EPD_SW, K=VGH
C3: EPD_SW <-> EPD_PUMP
D2: A=GND, K=EPD_PUMP
D1: A=EPD_PUMP, K=VGL
```

布局注意：

- L1、Q1、D1-D3、C1-C13 必须在 J2 后方形成紧凑区。
- EPD_SW、EPD_PUMP、VGH、VGL 铜皮只做满足连接的最小面积。
- VGH/VGL/VSH1/VSH2/VSL/VCOM 不跨越 U1、U7、USB、BME280 或按键区。
- SPI 信号可走 L1 或 L4，但全程有连续地参考；EPD_SCLK 不与 GNSS_RF 平行长距离走线。
- FPC 插拔区内不放高器件，不让电池线压住翻盖。

### 6.12 U8 ZY-BME280 模块

| U8 Pin | 丝印 | 网络 | 连接 |
|---:|---|---|---|
| 1 | VCC | BME_VCC_PAD | R23 0 ohm 接 3V3_MAIN，作为模块输入电源 |
| 2 | GND | GND | 地 |
| 3 | SCL | BME_SCL | U1 GPIO6，R22 4.7 kOhm 上拉 |
| 4 | SDA | BME_SDA | U1 GPIO5，R21 4.7 kOhm 上拉 |
| 5 | NC | NC | 悬空 |
| 6 | SDO | BME_SDO | 默认 R25 0 ohm 到 GND，地址 0x76 |
| 7 | GND | GND | 地 |
| 8 | 3.3V | BME_3V3_PAD | 模块板载稳压后的 3.3 V 输出，不是主板供电输入 |

选择电阻：

- R23 默认装：3V3_MAIN -> U8 Pin1/VCC。
- R24 默认不装：U8 Pin8/3.3V -> BME_3V3_TEST，仅用于预留测试；不进入 BOM。
- R25 默认装：U8 Pin6/SDO -> GND，I2C 地址 0x76。
- R26 默认不装：U8 Pin6/SDO -> U8 Pin8/3.3V，若改装为高地址选择；不进入 BOM。
- R25 和 R26 不得同时安装。

U8 应靠壳体通气孔，距 U3、U5、L1、L2、ESP 模块和 LED 导光柱至少约 8-10 mm。传感器周围不铺发热铜皮，不用胶完全封住传感器开孔。模块下方不放器件，避免底面零件或焊点顶起模块。

## 7. 阻容和分立器件网络速查

### 7.1 电阻

| 位号 | 值 | 网络 1 -> 网络 2 | 作用 |
|---|---:|---|---|
| R1 | 10 kOhm | GDR -> GND | EPD GDR 下拉 |
| R2 | 2.2 ohm | RESE -> GND | EPD 电流检测/参考 |
| R3/R4 | 5.1 kOhm | USB_CC1/CC2 -> GND | USB-C Rd |
| R5/R6 | 22 ohm | USB_DP/DM_CONN -> USB_DP/DM_MCU | USB 串联阻尼 |
| R7 | 2.21 kOhm | BQ_ISET -> GND | 约 400 mA 充电电流 |
| R8 | 1.5 kOhm | BQ_ILIM -> GND | 可编程限流预留 |
| R9 | 10 kOhm | BQ_TS -> GND | 两线电池 TS 固定偏置 |
| R10 | 3.3 kOhm | SYS_OUT -> CHG_LED_A | 红色充电灯限流 |
| R11 | 100 kOhm | 3V3_MAIN -> CHG_SENSE | 充电状态上拉 |
| R12 | 10 kOhm | 3V3_MAIN -> PGOOD | PGOOD 上拉 |
| R13 | 100 kOhm | STM_EN_N -> Q2_BASE | Q2 基极限流 |
| R14 | 100 kOhm | SYS_OUT -> TPS_EN | U5 EN 上拉 |
| R15 | 1 MOhm | Q2_BASE -> GND | Q2 基极下拉 |
| R16/R17 | 10 kOhm | 3V3_MAIN -> PWR_PBOUT/PWR_INT | U4 状态上拉 |
| R18 | 10 kOhm | 3V3_MAIN -> ESP_EN | ESP EN 上拉 |
| R19 | 100 kOhm | 3V3_MAIN -> LED_N | 绿灯控制节点默认上拉 |
| R20 | 4.7 kOhm | 3V3_MAIN -> PWR_LED_A | 绿灯限流 |
| R21/R22 | 4.7 kOhm | 3V3_MAIN -> BME_SDA/SCL | I2C 上拉 |
| R23 | 0 ohm | 3V3_MAIN -> BME_VCC_PAD | U8 默认供电，装配 |
| R24 | 0 ohm | BME_3V3_PAD -> BME_3V3_TEST | DNP，不进 BOM |
| R25 | 0 ohm | BME_SDO -> GND | U8 默认地址，装配 |
| R26 | 0 ohm | BME_SDO -> BME_3V3_PAD | DNP，不进 BOM |
| R27 | 100 kOhm | GNSS_EN -> GND | U6 默认关闭 |
| R28/R29 | 1 MOhm / 330 kOhm | BAT -> BAT_ADC -> GND | 电池电压分压 |
| R30/R31 | 10 kOhm | 3V3_MAIN -> FUNC1_BOOT/FUNC2_GPIO | 功能键上拉 |
| R32 | 100 kOhm | PSHOLD -> GND | 上电默认不保持 |

### 7.2 电容

| 位号 | 值 | 网络 | 必须靠近 |
|---|---:|---|---|
| C1/C2/C3/C5/C9/C11 | 4.7 uF/25 V | EPD 高压泵相关 | J2、D1-D3、L1、Q1 |
| C4 | 22 uF | 3V3_MAIN | EPD L1 输入侧 |
| C6 | 22 uF | J2 VCC/3V3_MAIN | J2 Pin16 |
| C7 | 0.1 uF | J2 VCC/3V3_MAIN | J2 Pin16 |
| C8 | 1 uF | EPD_VDD | J2 Pin18 |
| C10/C12/C13 | 1 uF | VGH/VGL/VCOM | J2 对应高压脚 |
| C14 | 1 uF | USB_5V | U3 IN |
| C15 | 10 uF | BAT | U3 BAT |
| C16 | 10 uF | SYS_OUT | U3 OUT |
| C17 | 0.1 uF | SYS_OUT | U4 VCC |
| C18 | 1 uF | STM_VREF | U4 VREF |
| C19 | 0.47 uF | STM_CSRD | U4 CSRD |
| C20/C21 | 10 uF | SYS_OUT | U5 VIN/PGND |
| C22 | 0.1 uF | SYS_OUT | U5 VINA/GND |
| C23-C25 | 22 uF each | 3V3_MAIN | U5 VOUT/PGND |
| C26/C27 | 10 uF + 0.1 uF | 3V3_MAIN | U1 3V3/GND |
| C28 | 1 uF | ESP_EN | U1 EN/GND |
| C29 | 0.1 uF | BAT_ADC | U1 GPIO4/GND |
| C30/C31 | 1 uF each | U6 IN/OUT | U6 |
| C32/C33 | 10 uF + 0.1 uF | 3V3_GNSS | U7 VCC/GND |
| C34 | 56 pF | GNSS_RF -> GNSS_ANT | U7 与 J3 之间 |

## 8. 各类走线规则

### 8.1 电源和地

- USB_5V、BAT、SYS_OUT、3V3_MAIN 主干优先使用铜皮；若用线，布局初期目标不小于约 0.6-0.8 mm，U3/U5 局部高电流连接尽量更宽。最终按铜厚、温升和实际峰值电流复核。
- 3V3_GNSS 可用较窄电源线，但 U6 到 U7 仍建议至少约 0.3 mm，并避开开关节点。
- 每颗去耦电容的地端旁放独立地过孔，不让多个电容串在同一条细地线上。
- L2/L5 为连续地，顶层各功能区地铜通过过孔接入；板边和接口周围做地过孔缝合。
- U3/U5 的“小信号地”和“功率地”通过布局控制回流，在芯片附近汇合，不要实际切断整个 GND 平面。

### 8.2 USB

- `USB_DP_CONN/USB_DM_CONN` 和 `USB_DP_MCU/USB_DM_MCU` 作为同一差分通道管理。
- L1 参考 L2，90 ohm 差分；线宽/间距用板厂叠层计算。
- 不跨地分割，不从 U5 电感、EPD_SW、GNSS 天线或 ESP 天线下方经过。
- R5/R6 靠 U1；U2 靠 J1；在这两个固定点之间用最直接路径连接。
- 尽量 0 次换层；必须换层时两根同时换层，并放回流地过孔。

### 8.3 GNSS RF

- `GNSS_RF` 和 `GNSS_ANT` 合起来是一条 50 ohm 单端馈线，中间仅串 C34。
- 全程 L1，参考 L2；不换层、不打测试点、不形成 T 形支线。
- 馈线同层两侧地铜与线的间距必须进入阻抗计算，不能画完后随意改变。
- 地过孔围栏目标间距约 1-2 mm，但不得侵入天线厂家要求的馈点/地面几何。
- RF 线和 U5/L2、EPD L1/Q1、USB、UART、时钟线保持尽可能大的间距。

### 8.4 EPD 高压与 SPI

- EPD_SW、EPD_PUMP 是主要噪声节点，铜皮小、线短，不走内层长线。
- VGH/VGL/VSH/VSL/VCOM 走线可用约 0.2-0.25 mm 起步，和其他信号保持至少约 0.25-0.30 mm；最终以板厂 DRC 和实际最高电压复核。
- SPI 的 SCLK、MOSI、CS、DC、RST 和 BUSY 保持连续地参考；不要为了等长增加蛇形。
- J2 附近高压电容的地回流直接下到 L2，不绕到 U5 地区。

### 8.5 I2C、UART、按键和 ADC

- BME_SDA/SCL 是低速 I2C，长度不敏感，但不要靠 GNSS_RF 平行走线；R21/R22 放在 U8 周围。
- GNSS UART 和 1PPS 可走 L4，参考 L5；不要从天线正下方或 U5 电感下方穿过。
- U0TXD/U0RXD 走线短，靠地包围；若用于烧录/调试，增加明确的 TP_TX、TP_RX、TP_GND 焊盘。
- BAT_ADC 是高阻节点，R28/R29/C29 靠 U1 GPIO4，BAT_ADC 铜皮小，不邻近 EPD_SW/TPS_L1/TPS_L2。
- 侧按键线靠板边走，但避免从 ESP/GNSS 天线净空区穿过。

## 9. 测试点和当前三个 DRC 警告

当前三个警告是 `BME_3V3_TEST`、`U0TXD`、`U0RXD` 仅连接一个元件引脚。网络本身不是错误，但警告不会自动生成 PCB 测试焊盘。

建议 PCB 前在原理图中增加以下测试点器件：

- TP_GND：至少 2 个。
- TP_USB_5V、TP_BAT、TP_SYS_OUT、TP_3V3_MAIN、TP_3V3_GNSS。
- TP_ESP_EN、TP_PSHOLD、TP_GNSS_EN。
- TP_U0TXD、TP_U0RXD，旁边配一个 GND。
- 可选 TP_GNSS_TXD、TP_GNSS_RXD、TP_1PPS。
- BME_3V3_TEST 只有在确实需要测模块 3.3 V 输出时才保留；否则可以让 R24 的测试端不进入 PCB，减少无意义警告。

测试点不要放在 GNSS/ESP 天线区、USB 差分线上或任何开关节点上。GNSS_RF 禁止测试点。

## 10. 0.6 mm 单面装配的机械和制造注意事项

- 0.6 mm 六层板很薄，铜分布要尽量对称，减少翘曲；大面积铜在 L2/L5 保持均衡。
- J1、J2 和电池焊线区域是主要受力点，应靠近外壳支撑筋或增加局部机械支撑。
- J1 的壳脚/槽孔、J2 固定脚和电池焊线孔可能包含通孔工艺。“单面贴片”应理解为元件只装一面，不等于禁止所有通孔结构。
- 所有器件高度以“PCB 上表面到器件最高点”计算，并加上锡膏、模块翘曲、双面胶和外壳公差；不能只比较标称器件高度与 4.3 mm。
- U8 和 J3 必须拿实物做高度、焊盘和外壳干涉复测。
- BME280 开孔不要被防水泡棉、导光柱或电池胶带堵住。
- 屏幕 FPC 要有自然弯折半径和插拔空间，不能在装配后被电池或背板硬压折。
- 先做一套机械样板或少量首板，检查 USB 插拔、按键手感、FPC 方向、电池线拉力、天线高度和整机闭合。

## 11. 建议的实际布板顺序

1. 从中框 STEP/DXF 得到最终板框、孔位、卡扣和禁布区。
2. 锁定 J1、J2、SW1-SW3、J3、U1 天线端和电池焊线位置。
3. 完成 J1-U2-D4-R3/R4 的 USB 入口区。
4. 完成 U3-C14/C15/C16-R7/R8/R9 的充电区。
5. 完成 U5-L2-C20-C25 的开关电源岛，先检查所有功率回路面积。
6. 完成 J2 周围 EPD 高压泵，先走 EPD_SW/EPD_PUMP/VGH/VGL。
7. 完成 U7-C34-J3 射频直线和天线地结构，再放 U6/C30-C33。
8. 放 U1 去耦、EN、USB 串联电阻及其余 GPIO 支线。
9. 放 U8、按键、LED、ADC 和低速控制器件。
10. 铺 L2/L5 完整地，建立 L3 电源分区，再布 L4/L6 低速线。
11. 计算并设置 USB 90 ohm、GNSS 50 ohm 规则后再正式走这两类线。
12. 增加测试点、地过孔、热过孔和板边缝合孔。
13. 运行原理图 DRC、更新 PCB、检查未布网络、PCB DRC、3D 干涉和装配高度。

## 12. 投板前必须关闭的项目

以下项目未关闭前，不应直接下单批量生产：

1. 取得并核对 `BWGNSCNX18-18W2` 原厂落地图，确认 J3 馈点、底部接地和地平面要求。
2. 为电池红黑线建立真实 PCB 焊盘/拉力释放结构；当前 BT1 不进入 PCB。
3. 确认 J2 Pin1、连接器开口和屏幕金手指接触面完全一致。
4. 选择板厂的 0.6 mm 六层阻抗叠层，重新计算 USB 90 ohm 和 GNSS 50 ohm 几何。
5. 按实物复测 U8 焊盘中心距、板体尺寸和高度。
6. 核对 D1-D7、Q1、Q2 的采购封装 Pin 编号和 A/K、G/S/D、B/E/C 极性。
7. 增加必要的电源、UART 和控制测试点；确认是否保留 BME_3V3_TEST。
8. 在真实屏幕、电池、背板和中框装配状态下验证 Wi-Fi/BLE 和 GNSS 性能。
9. 进行首板上电限流测试，再依次验证 USB、充电、3V3、开关机、ESP、EPD、BME280、GNSS。

## 13. 资料依据

- 当前在线 V7 原理图及 2026-09-01 网络导出结果。
- `参考资料/OSPTEK/EPD0426A02/docs/EPD_0426_A02_f14fa2c9ce.pdf`
- `参考资料/OSPTEK/EPD0426A02/docs/4.26寸墨水屏转接板.pdf`
- `参考资料/OSPTEK/EPD0426A02/docs/SSD_1677_447731c055.pdf`
- `tmp/pdfs/bq24074/BQ24074_TI.pdf`
- `tmp/pdfs/usb_candidate/TYPE-C-31-M-12_datasheet.pdf`
- TI TPS63021 数据手册：<https://www.ti.com/lit/ds/symlink/tps63021.pdf>
- Espressif ESP32-S3 PCB Layout Design：<https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/pcb-layout-design.html>

本说明记录的是当前 V7 的接线和布局意图。后续若修改原理图、封装或天线型号，必须同步更新本文，不能只改 PCB。
