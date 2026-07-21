# dload 占位符加载内容说明

本文件用于说明当前项目中各个函数内 `dload` 占位符实际应加载的数据内容，便于测试同学准备输入数据与核对外部访存流。

## 与当前交付流程的关系

- `src/main.cpp` 通过 `inline_asm_codegen` 生成下述算子的 ASM，仍是 HPU 指令流入口。
- `test/reference/main.cpp` 通过 `hpu_reference_vectors` 生成对应输入、常量、期望输出和人工可读 `.hex.txt`。
- 完整密文乘法的 HPU_MEM 镜像入口见 `outputs/ciphertext_multiply/test_data/memory_map.json`，逐对象 256B line offset/count 见 `hardware/line_map.csv`，阶段搬运顺序见同目录 `dma_plan.csv`。
- `hardware/hpu_mem_config.json` 已给出 base/size 和语义 CSR 编程顺序；CSR 数字偏移及 `line_map.csv` 到指令 `rs1/rs2` 的寄存器绑定仍需 runtime/RTL 确认。代码中的 `x0` 和符号地址寄存器必须据此替换或重定位。

## 通用约定

- `dload(rs1, rs2, pobj, type)` 仅代表“外部访存读取到片上对象槽位”的占位行为：
  - `type = mod_ctx`：将模数和 Barrett 参数安装到 Bank 5 固定模上下文表；`pobj` 是 DMA 传输句柄。
  - `type = poly`：加载多项式/RNS 通道数据或预计算常量（如 twiddle、qhat_inv 等）。
- 代码中 `x0` / `x_offset` / `x_c0` / `x_ct1_up` / `x_ct1_ntt` / `x_evk` / `x_out` / `x_tmp_c0` 等仅是占位地址寄存器名，测试时需用实际的 DMA/HBM 地址替代。
- `pmodld MOD_ID` 不读取 `pobj`，而是按 8-bit `MOD_ID` 从固定模表中选择上下文；模表中 context 的物理顺序必须与生成器使用的 `MOD_ID` 一致。
- `dload` 使传输目标槽位进入 live 状态。生成器在只读输入、常量、twiddle 和模表传输句柄最后一次使用后发出 `pfree`；输出使用 `dstore rel=1` 时由 DMA 完成后释放，不再重复发出 `pfree`。
- 数学 golden 使用 little-endian `uint64`；`dload` 应使用 `test_data/hardware/` 下 little-endian `uint32`、按 256B line 补齐的独立镜像或完整 `hpu_mem_image.u32.bin`。

---

## `generate_hpu_bconv_body_asm`
来源: [`src/util/bconv.cpp`](../src/util/bconv.cpp)

### dload 映射
![alt text](c0aaddcf5efa1bb152663b37b241c003.png)

| 位置 | 目标槽位 | 加载内容 | 说明 |
| --- | --- | --- | --- |
| 预处理阶段开头 | `POBJ_MOD_CTX` | **完整固定模表镜像**（输入 Q 与目标 P） | `POBJ_MOD_CTX` 是传输句柄；后续按 `MOD_ID` 选择 `q_j / p_i` |
| Stage 1: 每个 `q_j` | `POBJ_TMP_A` | `a_j`（输入多项式在 `q_j` 上的通道） | 注释中 `a_j` |
| Stage 1: 每个 `q_j` | `POBJ_TMP_B` | `qhat_inv_j` | 用于 `a_j * qhat_inv_j mod q_j` |
| Stage 2: 每个 `p_i`、每个 `q_j` | `POBJ_TMP_A` | `x_j`（Stage 1 输出的临时结果） | 注释中 `x_j` |
| Stage 2: 每个 `p_i`、每个 `q_j` | `POBJ_TMP_B` | `qhat_modp_j_i` | 预计算常量 |

> 备注：`generate_hpu_modup_body_asm` 直接复用 BConv 的 dload 语义。

---

## `generate_hpu_modup_body_asm`
来源: [`src/poly/modup.cpp`](../src/poly/modup.cpp)

### dload 映射

- 完全等同于 **BConv Q -> P** 的 dload 行为（见上节）。
- 其中 `num_q_digit` 与 `q_offset` 控制输入基的“切片范围”。

---

## `generate_hpu_moddown_body_asm`
来源: [`src/poly/moddown.cpp`](../src/poly/moddown.cpp)
![alt text](<截屏2026-05-15 08.47.51.png>)
### dload 映射

**Stage 1（BConv P -> Q，生成 correction term）**

- dload 行为与 BConv 相同，但“输入基”为 `P`，“目标基”为 `Q`。
- `q_j / qhat_inv_j / x_j / qhat_modp_j_i` 均应理解为 P->Q 场景对应的常量与中间值。

**Stage 2（修正并降模）**

| 位置 | 目标槽位 | 加载内容 | 说明 |
| --- | --- | --- | --- |
| Stage 2 每个 `q_i` | `POBJ_MOD_CTX` | **完整 Q 模表镜像** | 传输句柄；`pmodld MOD_ID` 切换 `q_i` |
| Stage 2 每个 `q_i` | `POBJ_Q` | `q` 基下的当前密文分量 | 被修正的输入 |
| Stage 2 每个 `q_i` | `POBJ_CORR` | correction term（由 Stage 1 产生） | `q - corr` |
| Stage 2 每个 `q_i` | `POBJ_P_INV` | `P^{-1} mod q_i` | 用于乘回缩放 |

---

## `generate_hpu_cmult_body_asm`
来源: [`src/poly/cmult.cpp`](../src/poly/cmult.cpp)

### dload 映射

| 位置 | 目标槽位 | 加载内容 | 说明 |
| --- | --- | --- | --- |
| 每个 `q_i` | `POBJ_MOD_CTX` | **完整 Q 模表镜像** | 传输句柄；`pmodld MOD_ID` 选择 `q_i` |
| 乘 `a0*b0` | `POBJ_A` / `POBJ_B` | `a0` / `b0` | 同一 `q_i` 基 |
| 乘 `a1*b1` | `POBJ_A` / `POBJ_B` | `a1` / `b1` | 同一 `q_i` 基 |
| 乘 `a0*b1` | `POBJ_A` / `POBJ_B` | `a0` / `b1` | 同一 `q_i` 基 |
| 乘 `a1*b0` | `POBJ_A` / `POBJ_B` | `a1` / `b0` | 同一 `q_i` 基 |

---

## `generate_hpu_pmult_body_asm`
来源: [`src/poly/pmult.cpp`](../src/poly/pmult.cpp)

### dload 映射

| 位置 | 目标槽位 | 加载内容 | 说明 |
| --- | --- | --- | --- |
| 开头 | `POBJ_MOD_CTX` | **完整 Q 模表镜像** | 传输句柄；`pmodld MOD_ID` 切换 `q_i` |
| 每个 `q_i` 第一次 | `POBJ_CT` | `ct0`（第 0 分量） | 同一 `q_i` 基 |
| 每个 `q_i` 第一次 | `POBJ_PT` | `pt`（明文多项式） | 同一 `q_i` 基 |
| 每个 `q_i` 第二次 | `POBJ_CT` | `ct1`（第 1 分量） | 同一 `q_i` 基 |

> 注：`POBJ_PT` 只在第一次读取，若测试数据分开存放，需确保读指针一致或显式复用。

---

## `generate_hpu_ntt_body_asm` / `generate_hpu_intt_body_asm`
来源: [`src/util/ntt.cpp`](../src/util/ntt.cpp)

### dload 映射

| 位置 | 目标槽位 | 加载内容 | 说明 |
| --- | --- | --- | --- |
| 每个 stage | `twiddle_obj` | NTT/INTT twiddle 表 | 每个 stage 都会加载一次 |

> 备注：函数为**原地变换**，第一个对象槽位为数据对象，第二个对象槽位为 twiddle。函数内对模上下文只给出注释占位，实际 `mod_ctx` 需由调用方在外层加载。

---

## `generate_hpu_keyswitch_body_asm`
来源: [`src/operator/keyswitch.cpp`](../src/operator/keyswitch.cpp)

### dload 映射（核心步骤）

**Step 1: ModUp（Q -> P）**

- 复用 `generate_hpu_modup_body_asm` 的 dload 语义（见上文）。

**Step 2: NTT on Q & P**

| 位置 | 目标槽位 | 加载内容 | 说明 |
| --- | --- | --- | --- |
| 每个基 | `POBJ_TMP_A` | `ct1_up` 的当前基分片 | ModUp 输出的切片 |
| 每个基 | `TWIDDLE` | NTT twiddle 表 | 供 `pntt` 使用 |

**Step 3: Multiply with EVK**

| 位置 | 目标槽位 | 加载内容 | 说明 |
| --- | --- | --- | --- |
| 每个基、每个 `v` | `POBJ_CT` | `ct1_ntt` 当前基分片 | NTT 后的 ct1 |
| 每个基、每个 `v` | `POBJ_EVK` | `evk[v]` 当前基分片 | 评估密钥 |
| 非首 digit | `POBJ_OUT` | 累加中间值 | 来自上一次 digit 结果 |

**Step 4: INTT on Q & P**

| 位置 | 目标槽位 | 加载内容 | 说明 |
| --- | --- | --- | --- |
| 每个基、每个 `v` | `POBJ_TMP_A2` | `out[v]` 当前基分片 | 乘加后的结果 |
| 每个基、每个 `v` | `TWIDDLE2` | INTT twiddle 表 | 供 `pintt` 使用 |

**Step 5: ModDown**

- 复用 `generate_hpu_moddown_body_asm` 的 dload 语义（见上文）。

**Step 6: Add c0 to out0**

| 位置 | 目标槽位 | 加载内容 | 说明 |
| --- | --- | --- | --- |
| 每个 `q_i` | `POBJ_OUT0` | `out0`（ModDown 后） | 当前基分片 |
| 每个 `q_i` | `POBJ_C0` | 原始 `c0` 分量 | 当前基分片 |

---

## `generate_hpu_ciphertext_multiply_body_asm`
来源: [`src/operator/ciphertext_multiply.cpp`](../src/operator/ciphertext_multiply.cpp)

完整乘法遵循 RLWE 密文乘法与重线性化流程，不等同于仅计算三分量张量积的 `cmult`：

1. 对两个输入密文的 `c0/c1` 分量执行 NTT。
2. 在每个 `q_i` 上计算 `t0=a0*b0`、`t1=a0*b1+a1*b0`、`t2=a1*b1`。
3. 将 `t0/t1/t2` 执行 INTT，得到 Q 基系数域三分量密文。
4. 按 digit 分解 `t2`，执行 Q -> Q∪P 的 ModUp 和 NTT。
5. 与重线性化密钥两个分量逐点乘并跨 digit 累加，再执行 INTT 和 Q∪P -> Q ModDown。
6. 将 key-switch 结果分别加到 `t0/t1`，输出重新线性化后的二分量密文。

### 主要 dload/dstore 数据

| 阶段 | 加载内容 | 存储内容 |
| --- | --- | --- |
| 输入 NTT | `ct_a_q`、`ct_b_q`、Q 模上下文、NTT twiddle | 两个密文的 NTT 域分量 |
| 张量积 | NTT 域 `a0/a1/b0/b1` | NTT 域 `t0/t1/t2` |
| 张量积 INTT | `t0/t1/t2`、Q 模上下文、INTT twiddle | 系数域 `t0/t1/t2` |
| digit ModUp/NTT | `t2` digit、Q∪P 模上下文、BConv 常量、twiddle | Q∪P 上的 digit NTT |
| EVK 乘加 | digit NTT、`relinearization_key_ntt_qp`、累加中间值 | Q∪P 上两个 key-switch 分量 |
| INTT/ModDown | 两个累加分量、Q∪P 常量与 twiddle | Q 基 key-switch correction |
| 最终合并 | `t0/t1` 与两个 correction | `ciphertext_out_q` |

Reference 中上述逻辑对象的文件、shape 和 checksum 见 `outputs/ciphertext_multiply/test_data/artifact_manifest.csv`；硬件 `uint32` 文件、checksum、line offset/count 分别见 `hardware/hardware_manifest.csv` 和 `hardware/line_map.csv`。当前生成器中的 `dload("x0", "x0", ...)` / `dstore("x0", "x0", ...)` 只表达数据依赖与计算顺序，尚未把这些 line 参数写入物理寄存器。

---

## `generate_hpu_auto_body_asm`
来源: [`src/poly/auto.cpp`](../src/poly/auto.cpp)

### dload 映射（核心步骤）

**Step 0: c0 的 NTT -> iNTT_auto**

| 位置 | 目标槽位 | 加载内容 | 说明 |
| --- | --- | --- | --- |
| 每个 `q_i` | `SLOT_A` | `c0` 当前基分片 | 来自 `x_c0/x_offset` |
| 每个 `q_i` | `TWIDDLE` | NTT twiddle 表 | NTT 使用 |
| 每个 `q_i` | `TWIDDLE` | 融合 auto 的 iNTT twiddle | iNTT 使用 |

**Step 1: ModUp**

- 复用 `generate_hpu_modup_body_asm` 的 dload 语义（见上文）。

**Step 2: Fused NTT Auto**

| 位置 | 目标槽位 | 加载内容 | 说明 |
| --- | --- | --- | --- |
| 每个基 | `SLOT_A` | `ct1_up` 当前基分片 | ModUp 输出 |
| 每个基 | `TWIDDLE` | NTT twiddle 表 | 融合 auto 的 NTT |

**Step 3: Multiply & Accumulate with EVK**

| 位置 | 目标槽位 | 加载内容 | 说明 |
| --- | --- | --- | --- |
| 每个基、每个 `v` | `SLOT_A` | `ct1_ntt` 当前基分片 | NTT 后 ct1 |
| 每个基、每个 `v` | `SLOT_B` | `evk[v]` 当前基分片 | 评估密钥 |
| 非首 digit | `SLOT_C` | 先前累加值 | 来自 `x_out` |

**Step 4: INTT**

| 位置 | 目标槽位 | 加载内容 | 说明 |
| --- | --- | --- | --- |
| 每个基、每个 `v` | `SLOT_A` | `out[v]` 当前基分片 | 乘加后的结果 |
| 每个基、每个 `v` | `TWIDDLE` | INTT twiddle 表 | 供 iNTT 使用 |

**Step 5: ModDown**

- 复用 `generate_hpu_moddown_body_asm` 的 dload 语义（见上文）。

**Step 6: Final Merge**

| 位置 | 目标槽位 | 加载内容 | 说明 |
| --- | --- | --- | --- |
| 每个 `q_i` | `SLOT_A` | `out0`（ModDown 后） | 当前基分片 |
| 每个 `q_i` | `SLOT_B` | `c0_auto`（Step 0 暂存） | 来自 `x_tmp_c0` |

---

## 未显式包含 dload 的算子

- `generate_hpu_mm_body_asm` 仅执行 `pmul`，无 `dload`（见 [`src/util/mm.cpp`](../src/util/mm.cpp)）。
