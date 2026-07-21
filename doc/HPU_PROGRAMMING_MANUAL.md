# HPU 软件编程手册

版本：0.1  
适用实现：`inline-asm` 当前软件实现  
日期：2026-07-21

## 前言

本手册描述当前 `inline-asm` 仓库实际支持的 HPU 汇编语言、32-bit 指令编码和软件可见编程约定。章节组织参考 RISC-V 指令集手册：先定义编程模型和公共指令格式，再逐条说明指令的语法、操作、约束和编码示例。

本手册的规范对象是以下代码：

- `encode/include/instruction.hpp`：助记符、格式和结构化字段。
- `encode/src/parser.cpp`：文本语法和操作数检查。
- `encode/src/encoder.cpp`：32-bit 机器指令编码。
- `include/util/hpu_asm.hpp`：算子生成器使用的汇编封装。
- `src/`：算子级和完整密文乘法指令流。

除特别说明外，本文中的“当前实现”均指上述软件实现，而不是尚未冻结的最终 RTL ABI。当前 `pmodld` 已采用 8-bit `MOD_ID` 和固定模表语义；其余 32-bit 指令到 HPU 内部 26-bit 命令的 precode 映射及 custom1 核内接口仍需与硬件版本统一，详见 `HPU_LATEST_SPEC_AUDIT.md`。

## 1. 编程模型

### 1.1 指令类别

HPU 指令均为 32-bit 定长指令，使用两个 RISC-V custom opcode：

| 类别 | 低 7-bit opcode | 用途 |
| --- | --- | --- |
| `custom0` | `0001011` (`0x0B`) | 模运算、NTT/INTT、配置、同步和对象释放 |
| `custom1` | `0101011` (`0x2B`) | 外部存储器与 HPU 对象之间的数据搬运 |

当前软件 ISA 包含 11 条指令：

| 指令 | 格式 | OPC/DIR | 功能 |
| --- | --- | --- | --- |
| `padd` | AR3 | `OPC=0000` | 逐系数模加 |
| `psub` | AR3 | `OPC=0001` | 逐系数模减 |
| `pmul` | AR3 | `OPC=0010` | 逐系数模乘或小常数乘 |
| `pmac` | AR3 | `OPC=0011` | 逐系数模乘加 |
| `pntt` | STG | `OPC=0100` | 前向 NTT 的一个 stage |
| `pintt` | STG | `OPC=0101` | 逆向 NTT 的一个 stage |
| `pmodld` | MOD | `OPC=0110` | 按 `MOD_ID` 选择并激活固定模表项 |
| `psync` | SYNC | `OPC=0111` | 建立 HPU 指令流同步边界 |
| `pfree` | CFG | `OPC=1000` | 释放对象槽位 |
| `dload` | DMA | `DIR=0` | 外部存储器到 HPU 对象 |
| `dstore` | DMA | `DIR=1` | HPU 对象到外部存储器 |

`OPC=1001..1111` 保留。旧助记符 `pshcfg`、`pshuf`、`pseed` 和 `psample` 不属于当前 ISA，汇编器必须拒绝。

### 1.2 多项式对象槽位

HPU 软件使用 `p0` 到 `p7` 表示 8 个 3-bit 对象槽位。对象槽位不是 RISC-V 通用寄存器，而是 HPU 内部多项式、常量、twiddle 或配置对象的逻辑标识。

每个 live 对象至少具有以下软件可见属性：

- 对象类型，例如多项式、模上下文或 twiddle。
- 系数数据，硬件镜像使用 little-endian `uint32` canonical residue。
- 以 HPU line 为单位的长度。
- 生命周期状态：未分配、正在搬运、有效、忙或可释放。

编码器只检查对象编号为 0..7，不检查对象是否 live、长度是否匹配或数据域是否正确。这些条件由指令生成器、runtime 和硬件对象状态机共同保证。

### 1.3 数据粒度

当前硬件数据包采用以下基本布局：

```text
1 coefficient = 32 bit
1 HPU line     = 64 coefficients = 2048 bit = 256 byte
```

`dload/dstore` 对应的硬件数据来自 `test_data/hardware/` 下的 `uint32` 镜像。对象的 line offset 和 line count 记录在 `line_map.csv`。

### 1.4 活动模上下文

`padd`、`psub`、`pmul`、`pmac`、`pntt` 和 `pintt` 均使用当前活动模上下文。上下文至少包含：

```text
q  = 32-bit modulus
mu = floor(2^64 / q), supplied to Barrett reduction
```

模上下文表通过 `dload type=2` 安装到专用模表区域，`pmodld MOD_ID` 从该表选择当前上下文。执行计算指令前，软件必须先激活与当前 RNS limb 对应的上下文；切换 Q/P 基时必须重新执行 `pmodld`。

### 1.5 指令顺序和对象生命周期

当前生成器按程序顺序建立数据依赖，推荐遵循以下规则：

1. 使用对象前先执行对应 `dload`，并等待后端认为对象有效。
2. 先执行 `pmodld`，再发出使用该模数的计算指令。
3. `pmac` 的目的对象是读写操作数，执行前必须已有有效累加值。
4. `pntt/pintt` 的数据对象在软件层面是读写对象。
5. 不再使用的输入、常量、twiddle 和模上下文使用 `pfree` 释放。
6. 输出使用 `dstore rel=1` 写回时，由 DMA 完成路径释放，之后不得再次 `pfree` 同一对象。
7. 软件需要阶段完成边界时发出 `psync`。

## 2. 汇编语言约定

### 2.1 寄存器和立即数

| 写法 | 范围 | 含义 |
| --- | --- | --- |
| `p0`..`p7` | 0..7 | HPU 对象槽位 |
| `x0`..`x31` | 0..31 | custom1 使用的 RISC-V 通用寄存器编号 |
| `cimm8` | 0..255 | `pmul/pmac` 小立即数 |
| `stage/idx0/idx1` | 0..15，格式另有说明 | stage 或辅助索引 |
| `mod_id` | 0..255 | 固定模上下文表编号 |
| `tag` | 0..31 | 5-bit 同步标签 |

整数支持十进制和 `0x` 前缀十六进制。助记符不区分大小写，但对象和寄存器前缀应写成小写 `p`、`x`。

### 2.2 注释和源文件

解析器接受单条 ASM，也可以从生成的 C++ 内联汇编字符串中提取指令。支持以下注释：

```text
// comment
# comment
; comment
/* comment */
```

项目生成的 `.cpp` 文件是中间表示；当前编码流程由项目自身的 parser/encoder 产生 `.inst32`，不要求系统 GNU assembler 原生认识这些 HPU 助记符。

## 3. 32-bit 指令格式

### 3.1 AR3 格式

```text
 31      28 27    25 24    22 21             14 13       10 9      7 6       0
+----------+--------+--------+-----------------+-----------+---------+---------+
|   OPC4   |  PDST  | PSRC1  |      OP2_8      |   MODE4   |  FLAG3  | 0001011 |
+----------+--------+--------+-----------------+-----------+---------+---------+
```

编码公式：

```text
word = (OPC4 << 28) | (PDST << 25) | (PSRC1 << 22)
     | (OP2_8 << 14) | (MODE4 << 10) | (FLAG3 << 7) | 0x0B
```

- 对象模式：`OP2_8[2:0]=PSRC2`，高 5-bit 为 0，`MODE4=0`。
- 立即数模式：`OP2_8=cimm8`，编码器设置 `MODE4[0]=1`。
- 文本汇编不直接暴露 `FLAG3`，其值为 0。

### 3.2 STG 格式

```text
 31      28 27    25 24    22 21    18 17    14 13    10 9      7 6       0
+----------+--------+--------+--------+--------+--------+---------+---------+
|   OPC4   | PDATA  | PTWID  |  IDX0  |  IDX1  | MODE4  |  FLAG3  | 0001011 |
+----------+--------+--------+--------+--------+--------+---------+---------+
```

编码公式：

```text
word = (OPC4 << 28) | (PDATA << 25) | (PTWID << 22)
     | (IDX0 << 18) | (IDX1 << 14) | (MODE4 << 10)
     | (FLAG3 << 7) | 0x0B
```

当前汇编语法把 `IDX0` 用作 `stage`。文本汇编不暴露 `FLAG3`，其值为 0。

### 3.3 MOD 格式

```text
 31      28 27             22 21             14 13              7 6       0
+----------+-----------------+-----------------+------------------+---------+
|   0110   |   reserved=0    |     MOD_ID8     |    reserved=0    | 0001011 |
+----------+-----------------+-----------------+------------------+---------+
```

编码公式：

```text
word = (0b0110 << 28) | (MOD_ID8 << 14) | 0x0B
```

去掉 RISC-V 低 7-bit opcode 后，`MOD_ID8` 位于 26-bit HPU 命令的 `OP2_8[14:7]`。其余操作数字段必须为 0。

### 3.4 CFG 格式

```text
 31      28 27    25 24    22 21                                  7 6       0
+----------+--------+--------+--------------------------------------+---------+
|   OPC4   |  IDX0  |  IDX1  |                CFG15                 | 0001011 |
+----------+--------+--------+--------------------------------------+---------+
```

编码公式：

```text
word = (OPC4 << 28) | (IDX0 << 25) | (IDX1 << 22)
     | (CFG15 << 7) | 0x0B
```

`pfree` 复用 CFG 格式，但只接受一个对象操作数；`IDX1` 和 `CFG15` 必须为 0。

### 3.5 SYNC 格式

```text
 31      28 27       23 22    20 19                              7 6       0
+----------+-----------+--------+----------------------------------+---------+
|   0111   |   TAG5    | MODE3  |            reserved=0            | 0001011 |
+----------+-----------+--------+----------------------------------+---------+
```

编码公式：

```text
word = (0b0111 << 28) | (TAG5 << 23) | (MODE3 << 20) | 0x0B
```

当前生成器使用 `tag=0, mode=0`。非零值可被编码，但其硬件语义尚未在项目中定义。

### 3.6 DMA 格式

```text
 31             25 24    20 19    15 14 13    12 11     9 8   7 6       0
+-----------------+--------+--------+--+--------+---------+-----+---------+
|   reserved=0    |  RS2   |  RS1   |D | TYPE2  | OBJ_ID  |  0  | 0101011 |
+-----------------+--------+--------+--+--------+---------+-----+---------+
```

编码公式：

```text
word = (RS2 << 20) | (RS1 << 15) | (DIR << 14)
     | (TYPE2 << 12) | (OBJ_ID << 9) | 0x2B
```

当前项目保持上述 custom1 位域。`RS1/RS2` 编码的是寄存器编号，不是寄存器值。

## 4. 算术指令

本章伪代码使用：

```text
P[n][i]  对象 pn 的第 i 个系数
q        当前活动模数
L        对象的系数数量
```

除 `pmac` 外，算术指令的目的对象可以是新对象或可覆盖对象。编码器不检查对象别名和长度；后端必须保证源对象和目的对象具有兼容布局。

### 4.1 PADD - 多项式模加

**语法**

```asm
padd pdst, psrc1, psrc2
```

**操作**

```text
for i = 0 .. L-1:
    P[pdst][i] = (P[psrc1][i] + P[psrc2][i]) mod q
```

**编码**：AR3，`OPC4=0000`，只支持对象形式。

**示例**

```asm
padd p2, p0, p1       # 0x0400400B
```

### 4.2 PSUB - 多项式模减

**语法**

```asm
psub pdst, psrc1, psrc2
```

**操作**

```text
for i = 0 .. L-1:
    P[pdst][i] = (P[psrc1][i] - P[psrc2][i]) mod q
```

结果按 canonical residue 归一化到 `[0,q)`。

**编码**：AR3，`OPC4=0001`，只支持对象形式。

**示例**

```asm
psub p2, p0, p1       # 0x1400400B
```

### 4.3 PMUL - 多项式逐点模乘

**语法**

```asm
pmul pdst, psrc1, psrc2
pmul pdst, psrc1, cimm8
```

**对象模式操作**

```text
for i = 0 .. L-1:
    P[pdst][i] = P[psrc1][i] * P[psrc2][i] mod q
```

**立即数模式操作**

```text
for i = 0 .. L-1:
    P[pdst][i] = P[psrc1][i] * cimm8 mod q
```

对象模式设置 `MODE4=0`；立即数模式设置 `MODE4[0]=1`。`cimm8` 范围为 0..255。

**示例**

```asm
pmul p2, p0, p1       # 0x2400400B
pmul p2, p0, 255      # 0x243FC40B
```

### 4.4 PMAC - 多项式逐点模乘加

**语法**

```asm
pmac pdst, psrc1, psrc2
pmac pdst, psrc1, cimm8
```

**对象模式操作**

```text
for i = 0 .. L-1:
    P[pdst][i] = (P[pdst][i] + P[psrc1][i] * P[psrc2][i]) mod q
```

**立即数模式操作**

```text
for i = 0 .. L-1:
    P[pdst][i] = (P[pdst][i] + P[psrc1][i] * cimm8) mod q
```

`pdst` 是读写累加器，执行前必须已经包含当前模数下的有效数据。第一个累加项通常使用 `pmul` 初始化，后续项使用 `pmac`。

**示例**

```asm
pmac p2, p0, p1       # 0x3400400B
pmac p2, p0, 255      # 0x343FC40B
```

## 5. 变换指令

### 5.1 PNTT - 前向 NTT stage

**语法**

```asm
pntt pdata, ptwiddle, stage, idx1, mode
```

**操作**

```text
P[pdata] = NTT_STAGE(P[pdata], P[ptwiddle], stage, idx1, mode, q)
```

当前生成器把 `pdata` 视为同一 logical object id 下的读写对象，每条 `pntt` 只执行一个 stage。完整长度为 `N` 的 NTT 发出 `log2(N)` 条指令，stage 从 0 递增到 `log2(N)-1`。

| 操作数 | 范围 | 当前用法 |
| --- | --- | --- |
| `pdata` | `p0`..`p7` | 数据对象 |
| `ptwiddle` | `p0`..`p7` | 当前 stage 的 twiddle 对象 |
| `stage` | 0..15 | 编入 `IDX0` |
| `idx1` | 0..15 | 当前生成器写 0 |
| `mode` | 0..15 | 当前生成器写 0 |

**示例**

```asm
pntt p0, p3, 15, 0, 0    # 0x40FC000B
```

软件通常在每个 stage 前加载 twiddle，并在该 stage 后释放：

```asm
dload x12, x13, p3, 1
pntt  p0, p3, 0, 0, 0
pfree p3
```

### 5.2 PINTT - 逆向 NTT stage

**语法**

```asm
pintt pdata, ptwiddle, stage, idx1, mode
```

**操作**

```text
P[pdata] = INTT_STAGE(P[pdata], P[ptwiddle], stage, idx1, mode, q)
```

操作数范围和生命周期与 `pntt` 相同。完整 INTT 需要发出 `log2(N)` 个 stage；归一化、逆 twist 和物理数据排列必须与提供给硬件的 twiddle ABI 一致。

**示例**

```asm
pintt p0, p3, 15, 0, 0   # 0x50FC000B
```

## 6. 配置与生命周期指令

### 6.1 PMODLD - 激活模上下文

**语法**

```asm
pmodld mod_id
```

**操作**

```text
line = MOD_TABLE_BASE_LINE + (mod_id >> 4)
slot = mod_id & 0xF
active_mod_context = MOD_TABLE[line][slot]
```

| 操作数 | 范围 | 含义 |
| --- | --- | --- |
| `mod_id` | 0..255 | Bank 5 固定模上下文表中的 8-bit 表项编号 |

一条 256B HPU line 可容纳 16 个 128-bit 模上下文，因此 `mod_id[7:4]` 选择相对 line，`mod_id[3:0]` 选择 line 内 slot。`pmodld` 不读取普通对象槽位，也不产生多项式结果；它改变后续模运算使用的 q/Barrett mu，因此必须位于对应计算指令之前。

模表的数据搬入与上下文选择是两个独立步骤：`dload type=2` 负责安装模表数据，`pmodld` 只携带 `MOD_ID`。当前 custom1 仍使用对象号作为 DMA 传输句柄，但该对象号不会编码进 `pmodld`。

**示例**

```asm
pmodld 0              # 0x6000000B
pmodld 1              # 0x6000400B
pmodld 255            # 0x603FC00B
```

旧语法 `pmodld psrc, idx1, cfg` 已删除，汇编器必须拒绝。

### 6.2 PFREE - 释放对象

**语法**

```asm
pfree psrc
```

**操作**

```text
require OBJ[psrc] is allocated and not busy
release OBJ[psrc]
```

`pfree` 使用 CFG 格式，目标对象编码在 `IDX0`，`IDX1=0`、`CFG15=0`。它必须排在对象最后一次读取之后。

**示例**

```asm
pfree p4              # 0x8800000B
```

对已经使用 `dstore rel=1` 释放的对象再次执行 `pfree` 属于非法生命周期操作。

### 6.3 PSYNC - 同步边界

**语法**

```asm
psync tag, mode
```

**当前软件语义**

```text
wait_until_preceding_hpu_work_reaches_sync_boundary(tag, mode)
notify_software()
```

| 操作数 | 范围 | 建议值 |
| --- | --- | --- |
| `tag` | 0..31 | 0 |
| `mode` | 0..7 | 0 |

当前项目只依赖 `psync 0, 0` 的通用阶段屏障语义。非零 tag/mode 能被编码并用于 RV 字段测试，但尚无稳定的软件含义。`psync` 是否覆盖所有 custom1 DMA 完成事件仍需按目标 RTL 版本确认。

**示例**

```asm
psync 0, 0            # 0x7000000B
psync 31, 7           # 0x7FF0000B
```

## 7. 外部访存指令

### 7.1 DLOAD - 外部存储器加载

**语法**

```asm
dload rs1, rs2, pdst, load_type
```

**操作**

```text
enqueue_load(GPR[rs1], GPR[rs2], pdst, load_type)
on completion:
    OBJ[pdst] becomes valid
```

| `load_type` | 名称 | 当前项目用途 |
| --- | --- | --- |
| 0 | `seg` | 多项式片段/普通分段数据 |
| 1 | `poly` | 完整多项式、twiddle 或多项式常量 |
| 2 | `mod_ctx` | 模上下文集合 |
| 3 | `shuffle_cfg` | shuffle 配置；编码保留，旧 `pshuf` 指令已删除 |

`rs1` 和 `rs2` 是 5-bit RISC-V 寄存器编号。当前项目期望 runtime 将 HPU_MEM line offset/count 或目标平台定义的 DMA 参数放入对应寄存器；生成的完整算子流仍大量使用 `x0,x0` 占位，因此这些 `.inst32` 目前只表达计算顺序，不能直接作为有效 DMA 请求执行。

**示例**

```asm
dload x10, x11, p0, 0     # 0x00B5002B
```

### 7.2 DSTORE - 外部存储器写回

**语法**

```asm
dstore rs1, rs2, psrc, rel
```

**操作**

```text
enqueue_store(GPR[rs1], GPR[rs2], psrc)
on completion:
    if rel == 1:
        release OBJ[psrc]
```

| `rel` | 语义 |
| --- | --- |
| 0 | 写回完成后保留源对象 |
| 1 | 写回完成后释放源对象 |

`rel` 只允许 0 或 1。源对象必须已经有效，且在 DMA 读取期间不得被覆盖或提前 `pfree`。

**示例**

```asm
dstore x10, x11, p2, 1    # 0x00B5542B
```

## 8. 推荐编程序列

### 8.1 逐点模乘

以下序列展示一个 RNS limb 上的多项式逐点乘。寄存器中的实际 offset/count 由 runtime 准备：

```asm
dload  x10, x11, p4, 2     # install mod table; p4 is the DMA transfer handle
pmodld 0                   # activate q0 by MOD_ID
dload  x12, x13, p0, 1     # left polynomial
dload  x14, x15, p1, 1     # right polynomial
pmul   p2, p0, p1
pfree  p0
pfree  p1
dstore x16, x17, p2, 1
pfree  p4
psync  0, 0
```

### 8.2 乘加累积

```asm
pmul p2, p0, p1            # initialize accumulator
pmac p2, p3, p5            # p2 += p3 * p5
pmac p2, p6, 7             # p2 += p6 * 7
```

### 8.3 完整 NTT

对于 `N=4096`，当前生成器发出 12 个 stage：

```asm
# q already selected with pmodld
dload x10, x11, p0, 1      # polynomial

dload x12, x13, p3, 1      # stage 0 twiddle
pntt  p0, p3, 0, 0, 0
pfree p3

# repeat for stage 1 .. 11
dload x14, x15, p3, 1
pntt  p0, p3, 11, 0, 0
pfree p3

dstore x16, x17, p0, 1
psync 0, 0
```

### 8.4 RNS 循环

HPU 同一时刻只有一个活动模上下文。对 Q/P 多个 limb 执行同一算子时，软件按 limb 循环：

```text
for each modulus context i:
    pmodld i
    dload operands for limb i
    execute arithmetic/NTT instructions
    dstore result for limb i
```

不能在一次 `pmodld` 后混合处理不同模数的数据。

## 9. 汇编器检查和错误

当前汇编器在编码前检查：

- 助记符是否属于 11 条当前指令。
- 操作数数量是否正确。
- `p` 对象和 `x` 寄存器是否越界。
- stage、idx、mode、tag、cfg、立即数和 type/rel 是否在编码范围内。
- 只有 `pmul/pmac` 可以使用整数第三操作数。
- `pfree` 只能有一个对象操作数。
- `dstore rel` 只能为 0 或 1。

这些是软件编码错误，表现为 assembler 抛出错误，不等价于 RISC-V 运行时 trap。对象未加载、长度不匹配、模上下文错误、DMA 越界和 busy 冲突属于硬件/runtime 验证范围。

## 10. 构建与输出

生成指令流、编码和测试数据：

```bash
cmake -S . -B build
cmake --build build -j --target hpu_delivery
ctest --test-dir build --output-on-failure
```

主要输出：

| 文件 | 内容 |
| --- | --- |
| `outputs/<case>/<case>.asm` | HPU 汇编指令流 |
| `outputs/<case>/<case>.cpp` | C++ 内联汇编形式 |
| `outputs/<case>/<case>.inst32` | 每行一个 32-bit 二进制字符串 |
| `outputs/rv_interface_smoke/test_data/expected_decode.csv` | 指令字、路由和规范化汇编 |
| `outputs/<case>/test_data/hardware/` | `uint32` HPU 数据镜像和 line map |

`.inst32` 是文本形式的 32 个 `0/1` 字符，不是可直接按字节加载的 little-endian ELF 或 binary。接入硬件测试平台时应明确其文本解析或另行打包为目标字节序。

## 附录 A：指令编码速查

| 指令示例 | 32-bit 机器码 |
| --- | --- |
| `padd p2, p0, p1` | `0x0400400B` |
| `psub p2, p0, p1` | `0x1400400B` |
| `pmul p2, p0, p1` | `0x2400400B` |
| `pmul p2, p0, 255` | `0x243FC40B` |
| `pmac p2, p0, p1` | `0x3400400B` |
| `pmac p2, p0, 255` | `0x343FC40B` |
| `pntt p0, p3, 15, 0, 0` | `0x40FC000B` |
| `pintt p0, p3, 15, 0, 0` | `0x50FC000B` |
| `pmodld 0` | `0x6000000B` |
| `pmodld 255` | `0x603FC00B` |
| `psync 0, 0` | `0x7000000B` |
| `pfree p4` | `0x8800000B` |
| `dload x10, x11, p0, 0` | `0x00B5002B` |
| `dstore x10, x11, p2, 1` | `0x00B5542B` |

## 附录 B：当前实现边界

以下内容不由当前编码器单独保证：

1. 32-bit 指令到 HPU 26-bit 内部命令的最终 precode 映射。
2. custom1 如何将 `dload type=2` 的模表镜像绑定到 Bank 5 固定表区域。
3. custom1 中 `rs1/rs2` 的最终 line sideband 或 DTLB descriptor 语义。
4. `psync` 是否包含所有 DMA 完成事件。
5. NTT/INTT 的最终物理 in-place/out-of-place 和 twiddle lane 布局。
6. CSR、cache maintenance、中断和 fault 的 runtime 实现。

在上述接口冻结前，本手册可作为当前软件编码器和指令生成器的规范，但不能代替 SoC/RTL 接口控制文档。
