# HPU 内联汇编指令与机器码转译手册

本手册基于 `HPU_2.txt` 数据手册第二版，为负责后端机器码转译的开发人员提供 HPU 内联汇编指令的标准语法格式、操作数映射说明以及译码对照参考。

## 一、概述与编码规范

HPU 指令分两类操作码通道：
- **外部访存类指令 (custom1)**：用于启动主存与本地 SRAM 的数据搬运并维护对象槽位（如 `dload`, `dstore`）。
- **内部执行类指令 (custom0)**：由 HPU 内部依次派发执行（如 `padd`, `pntt`, `psync` 等）。操作码固定为 `0001011`。HPU内部将 `inst[31:28]` 用作扁平化的 4-bit 主操作码 (OPC)。

**核心约定**：
1. **统一本地SRAM与片段试图**：操作数 `p0, p1 ... p7` 表示本地 3-bit 对象槽位视图，而非大容量向量寄存器。
2. **即立即数模式隐含化**：为了可读性，部分基础模运算（AR3 格式）依据汇编习惯通过助记符直接区分寻址模式，带 `i` 后缀的指令表示末尾操作数为 8 位立即数 (`cimm8`)。在译码阶段需将其转译为 `MODE[0] = 1` 的指令。
3. **参数强约束**：所有的修饰位（如 `MODE/FLAG`、`IDX1`、`TAG`）全部作为明文参数写在汇编指令末尾。

---

## 二、指令格式与译码规则对照表

### 1. AR3 格式：三对象基础算术类
用于基础逐元素模运算类指令，HPU 并行度为 64。

本类指令在汇编中使用两组助记符区分对象的寻址方式。
* **操作数格式 (对象寻址, `mode=0`)**: `mnemonic pdst, psrc1, psrc2`
* **操作数格式 (立即数寻址, `mode=1`)**: `mnemonic p_dst, psrc1, cimm8`

| 汇编指令名称 | 操作数结构 | 核心语义 | 操作码 OPC + 译码说明 |
| --- | --- | --- | --- |
| `padd` | `pdst, psrc1, psrc2` | 模加（对象） | OPC对应padd，设置 `MODE[0]=0` |
| `paddi`| `pdst, psrc1, cimm8` | 模加（立即数） | OPC对应padd，设置 `MODE[0]=1` |
| `psub` | `pdst, psrc1, psrc2` | 模减（对象） | OPC对应psub，设置 `MODE[0]=0` |
| `psubi`| `pdst, psrc1, cimm8` | 模减（立即数） | OPC对应psub，设置 `MODE[0]=1` |
| `pmul` | `pdst, psrc1, psrc2` | 模乘（对象） | OPC对应pmul，设置 `MODE[0]=0` |
| `pmuli`| `pdst, psrc1, cimm8` | 模乘（立即数） | OPC对应pmul，设置 `MODE[0]=1` |
| `pmac` | `pdst, psrc1, psrc2` | 模乘加（对象） | OPC对应pmac，设置 `MODE[0]=0` |
| `pmaci`| `pdst, psrc1, cimm8` | 模乘加（立即数） | OPC对应pmac，设置 `MODE[0]=1` |

> *注：译码器遇到 `i` 结尾的助记符时，需提取第三个操作数作为 `cimm8` 填入 `OP2[7:0]` 字段中，不再需要后续提供单独的 MODE 字段。*

---

### 2. STG 格式：Stage / Transform 执行类
具有明显阶段性、流水化以及并行重排特征的指令操作（执行于 128 并行蝶形/重排模式）。

* **操作数格式**: `mnemonic pdst, psrc, field_3, idx1, mode`

| 汇编指令名称 | 操作数示例及意义 | 译码说明 |
| --- | --- | --- |
| `pntt` | `pntt pdst, psrc, stage, idx1, mode` | 作用于完整对象，`stage`占用专属4-bit |
| `pintt`| `pintt pdst, psrc, stage, idx1, mode` | 作用于完整对象，同上 |
| `pshuf`| `pshuf pdst, psrc, idx0, idx1, mode` | `idx0` 为Shuffle主模式，`idx1` 为参数化索引 |
| `psample`|`psample pdst, psrc, idx0, idx1, mode` | `psrc` 现阶段预留填0，`idx0`选择分布类型 |

---

### 3. CFG 格式：配置与控制类
用于刷新 HPU 内部由于容量限制而维护的小型上下文状态（模参数、Shuffle模式及随机数种子）。

* **操作数格式**: `mnemonic ...`

| 汇编指令名称 | 操作数示例及意义 | 译码说明 |
| --- | --- | --- |
| `pmodld` | `pmodld psrc, idx1, cfg` | 从 `psrc` 槽位加载模上下文数据。`cfg` 为 15-bit 控制字 |
| `pshcfg` | `pshcfg psrc, idx1, cfg` | 从 `psrc` 槽位装载 Shuffle 静态配置。 |
| `pseed`  | `pseed imm21` | 特殊格式：`inst[27:7]` 全部21位用于存放随机种子 `IMM21` |

---

### 4. SYNC 格式：同步屏障类
通过独立收敛控制与中断线向 RISC-V 核心提供同步。

* **操作数格式**: `psync tag, mode`

| 汇编指令名称 | 操作数示例及意义 | 译码说明 |
| --- | --- | --- |
| `psync` | `psync tag, mode` | 建立同步栅栏。遇到该指令时在此前所有指令对SRAM可见前停止发射后续指令。 |

---

### 5. custom1：外部访存操作
用于通过 DMA 与 HPU SRAM 交互并注册对象槽位生命周期。

* **操作数格式**: `mnemonic rs1, rs2, p_obj, arg4`

| 汇编指令名称 | 操作数示例及意义 | 译码说明 |
| --- | --- | --- |
| `dload` | `dload rs1, rs2, pdst, load_type` | `DIR=0`. `load_type` 00:普通片段, 01:完整对象, 10:模上下文, 11:重排对象 |
| `dstore`| `dstore rs1, rs2, psrc, rel` | `DIR=1`. `rel` 表示导出后是否释放槽位(0保留, 1释放) |

---
