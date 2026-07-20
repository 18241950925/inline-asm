# HPU 内联汇编指令与机器码转译手册

本手册描述当前项目采用的 HPU 软件接口。体系结构只保留 11 条指令：9 条 `custom0` 内部执行指令和 2 条 `custom1` DMA 指令。历史版本中的 `pshcfg`、`pshuf`、`pseed`、`psample` 已删除，编码器必须拒绝这些助记符。

> 本项目继续以现有 `custom1` 位域实现为准；本次指令集更新不改变 `dload/dstore` 的字段位置和 DLoad type 定义。

## 1. 指令总表

| 类别 | 指令 | 格式 | custom0 OPC | 语义 |
| --- | --- | --- | --- | --- |
| 算术 | `padd` | AR3 | `0000` | 模加 |
| 算术 | `psub` | AR3 | `0001` | 模减 |
| 算术 | `pmul` | AR3 | `0010` | 模乘 |
| 算术 | `pmac` | AR3 | `0011` | 模乘加 |
| 变换 | `pntt` | STG | `0100` | 原地 NTT stage |
| 变换 | `pintt` | STG | `0101` | 原地 INTT stage |
| 上下文 | `pmodld` | CFG | `0110` | 激活模上下文 |
| 同步 | `psync` | SYNC | `0111` | 队列同步/完成通知 |
| 生命周期 | `pfree` | CFG | `1000` | 释放对象槽位地址空间 |
| 外部访存 | `dload` | DMA | - | 外存加载到对象槽位 |
| 外部访存 | `dstore` | DMA | - | 对象槽位写回外存 |

`custom0` 的固定低 7-bit opcode 为 `0001011`，`inst[31:28]` 为表中的 OPC。`1001` 到 `1111` 保留，不得分配给旧指令。

## 2. AR3 算术指令

对象模式语法：

```asm
padd p2, p0, p1
psub p2, p0, p1
pmul p2, p0, p1
pmac p2, p0, p1
```

只有 `pmul` 和 `pmac` 支持 8-bit 小立即数，仍使用原助记符，由第三操作数类型选择 `MODE[0]=1`：

```asm
pmul p2, p0, 255
pmac p2, p0, 255
```

`paddi`、`psubi`、`pmuli`、`pmaci` 不是体系结构指令。当前解析器也不接受 `padd/psub` 的立即数形式。对象槽位范围为 `p0` 到 `p7`，`cimm8` 范围为 0 到 255。

## 3. STG 变换指令

```asm
pntt pdata, ptwiddle, stage, idx1, mode
pintt pdata, ptwiddle, stage, idx1, mode
```

`pdata` 是原地读写的数据对象，`ptwiddle` 是当前 stage 的 twiddle 对象。`stage`、`idx1` 和 `mode` 均为 4-bit 字段。生成器在每个 stage 执行以下生命周期：

```asm
dload ..., ptwiddle, 1
pntt pdata, ptwiddle, stage, idx1, mode
pfree ptwiddle
```

INTT 同理。`pfree` 只释放当前 stage 的 twiddle；`pdata` 仍保留给下一 stage 或后续 `dstore`。

## 4. CFG 指令

### `pmodld`

```asm
pmodld psrc, idx1, cfg
```

从 `psrc` 指向的对象中装载并激活模上下文。`idx1` 为 3-bit，`cfg` 为 15-bit。

### `pfree`

```asm
pfree psrc
```

`IDX0` 指定要释放的对象槽位，CFG 中其余字段必须为 0。`pfree` 按队列顺序执行，应放在该对象最后一次读取之后。示例 `pfree p4` 的当前编码为 `0x8800000B`。

## 5. SYNC 指令

```asm
psync tag, mode
```

`tag` 为 5-bit，`mode` 为 3-bit。`psync` 用于此前操作的完成边界；它不能代替对象生命周期管理，临时对象仍需在最后一次使用后显式 `pfree`。

## 6. custom1 DMA 指令

当前项目的语法和位域保持不变：

```asm
dload  rs1, rs2, pdst, load_type
dstore rs1, rs2, psrc, rel
```

| 指令 | 字段 | 约定 |
| --- | --- | --- |
| `dload` | `load_type` | `0=seg`、`1=poly`、`2=mod_ctx`、`3=shuffle_cfg` |
| `dstore` | `rel` | `0=写回后保留`、`1=写回完成后释放` |

`custom1` 固定低 7-bit opcode 为 `0101011`。当前编码布局由 `encode/src/encoder.cpp` 的 `encode_dma` 定义，本次更新没有修改。

## 7. 对象生命周期规则

1. `dload` 成功后，目标对象槽位进入 live 状态；再次写同一槽位前必须确保旧对象已释放。
2. 算术、变换和 `pmodld` 读取源对象，但不会自动释放它们。
3. 不需要写回的输入、常量、twiddle 和模上下文，在最后一次使用后执行 `pfree`。
4. 需要写回且之后不再使用的结果执行 `dstore ..., 1`，由 DMA 完成后释放，不能再对同一对象追加 `pfree`。
5. 仍要复用的对象执行 `dstore ..., 0` 或暂不释放，直到真实的最后一次使用。

完整算子流已按这些规则生成 `pfree`。RV 冒烟用例同时覆盖 `pfree` 正向编码，以及旧指令和非法 `pfree` 操作数的拒绝行为。
