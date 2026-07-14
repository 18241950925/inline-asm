# HPU 后端约束信息需求文档

本文档定义 HPU 作为编译器后端目标时必须提供的硬件信息。目标是让上层 IR、调度器、汇编生成器、编码器和测试框架可以像 LLVM/MLIR 对接不同 target 一样，通过一份稳定的 target contract 生成正确代码，而不是把硬件假设散落在算子实现里。

本文档不规定某一种具体实现方式。硬件团队可以用 YAML、JSON、TableGen、MLIR dialect attribute、C++ target description 或其他形式提供这些信息，但语义必须完整、可验证。

---

## 1. Target 基本描述

硬件后端首先需要提供一组全局目标属性，用于决定代码生成和合法化边界。

| 字段 | 必需性 | 说明 |
| --- | --- | --- |
| `target_name` | 必需 | 目标名，例如 `hpu-v1`、`hpu-v2` |
| `isa_version` | 必需 | 指令集版本，需和编码文档绑定 |
| `encoding_version` | 必需 | 机器码编码版本 |
| `word_bits` | 必需 | 指令字宽，目前项目为 32 bit |
| `endianness` | 必需 | 数据与指令编码端序 |
| `host_isa` | 必需 | 宿主侧 ISA，例如 RISC-V custom0/custom1 |
| `supported_schemes` | 建议 | 支持的 FHE 方案，如 BFV/BGV/CKKS |
| `max_instruction_count` | 建议 | 单 kernel 或单提交批次最大指令数 |
| `feature_flags` | 建议 | 是否支持 fused NTT、auto、采样、异步 DMA 等 |

示例：

```yaml
target:
  name: hpu-v1
  isa_version: 2
  encoding_version: 2026-05-15
  word_bits: 32
  host_isa: riscv-custom
```

---

## 2. 数据模型与 FHE 参数边界

FHE 编译器需要知道硬件能合法处理哪些多项式、RNS 和模数参数。

| 字段 | 必需性 | 说明 |
| --- | --- | --- |
| `poly_degree_supported` | 必需 | 支持的 `N` 列表或范围，且是否要求 2 的幂 |
| `coeff_bit_width` | 必需 | 单个系数的有效位宽 |
| `limb_bit_width` | 必需 | RNS prime limb 位宽 |
| `num_q_min/max` | 必需 | Q 基数量范围 |
| `num_p_min/max` | 必需 | P 基数量范围 |
| `dnum_constraints` | 必需 | digit decomposition 约束，如 `num_q % dnum == 0` |
| `ntt_prime_constraints` | 必需 | prime 是否必须满足 `q_i = 1 mod 2N` |
| `scale_constraints` | 方案相关 | CKKS scale 支持范围和 rescale 约束 |
| `plaintext_modulus_constraints` | 方案相关 | BFV/BGV 明文模数约束 |

必须明确的算法域：

| 对象 | 必须声明 |
| --- | --- |
| 密文输入 | coefficient domain 或 NTT domain |
| 密文输出 | coefficient domain 或 NTT domain |
| 评估密钥 | 是否预先在 NTT domain |
| twiddle 表 | bit-reversed、natural order 或硬件内部顺序 |
| ModUp/ModDown 中间值 | 使用 Q、P、Q union P 的哪种基顺序 |

当前项目建议约定：

```text
完整 ciphertext_multiply 输入: coefficient domain
张量积 cmult 输入: NTT/evaluation domain
KeySwitch 输入: coefficient domain
完整 ciphertext_multiply 输出: coefficient domain
```

---

## 3. 对象槽位与片上存储模型

类似 LLVM 需要知道 register file，HPU 后端必须知道对象槽位和本地 SRAM 的精确约束。

| 字段 | 必需性 | 说明 |
| --- | --- | --- |
| `object_slot_count` | 必需 | 对象槽位数量，目前汇编使用 `p0` 到 `p7` |
| `work_slot_count` | 必需 | 可同时用于多项式运算的槽位数量，当前复杂算子按 3 个工作槽位设计 |
| `slot_id_bits` | 必需 | 槽位寻址位宽，当前为 3 bit |
| `object_capacity` | 必需 | 一个对象槽位可容纳的数据规模 |
| `poly_layout_in_slot` | 必需 | 一个多项式对象在槽内的系数排列 |
| `mod_ctx_layout` | 必需 | 模上下文对象格式 |
| `shuffle_cfg_layout` | 必需 | shuffle 配置对象格式 |
| `twiddle_layout` | 必需 | NTT/INTT twiddle 对象格式 |
| `lifetime_rules` | 必需 | `dstore rel=1` 是否释放槽位，释放后何时可复用 |
| `aliasing_rules` | 必需 | `pdst` 是否允许和 `psrc` 相同，原地操作是否合法 |

需要给出每条指令的槽位读写集合。例如：

| 指令 | 读槽位 | 写槽位 | 是否允许原地 |
| --- | --- | --- | --- |
| `padd pdst, psrc1, psrc2` | `psrc1`, `psrc2` | `pdst` | 需声明 |
| `pmac pdst, psrc1, psrc2` | `pdst`, `psrc1`, `psrc2` | `pdst` | 是 |
| `pntt pdata, ptwiddle, ...` | `pdata`, `ptwiddle` | `pdata` | 是 |
| `dload ..., pdst, ...` | 外存 | `pdst` | 覆盖规则需声明 |
| `dstore ..., psrc, rel` | `psrc` | 外存 | `rel=1` 释放 |

---

## 4. 外部内存、DMA 与 ABI

这是当前项目最需要补齐的部分。编译器需要知道 `dload/dstore` 中 `rs1/rs2` 如何映射到真实地址、步长和数据对象。

| 字段 | 必需性 | 说明 |
| --- | --- | --- |
| `address_registers` | 必需 | 哪些 GPR 可作为 DMA 地址寄存器 |
| `dload_address_formula` | 必需 | `rs1`, `rs2`, `load_type` 如何计算实际地址 |
| `dstore_address_formula` | 必需 | `rs1`, `rs2`, `rel` 如何计算写回地址 |
| `alignment_bytes` | 必需 | 各类对象的地址对齐要求 |
| `stride_rules` | 必需 | RNS limb、component、stage、digit 的地址步长 |
| `memory_spaces` | 必需 | HBM/DDR/SRAM/constant memory 的编号与访问限制 |
| `dma_granularity` | 必需 | 单次搬运字节数、burst 限制 |
| `max_inflight_dma` | 必需 | 最大未完成 DMA 数量 |
| `dma_ordering` | 必需 | 同槽位、同地址、跨地址访问的顺序保证 |
| `cache_coherence` | 必需 | 与 host cache 是否一致，是否需要 flush/invalidate |
| `error_behavior` | 必需 | 越界、未对齐、非法类型的行为 |

建议定义一个统一 kernel ABI：

```text
rs1 = base pointer
rs2 = descriptor pointer or dynamic offset
arg4/load_type = object kind
```

并用 descriptor 描述每类数据：

```yaml
descriptor:
  ct_a:
    base: input0
    layout: [component, q_limb, coeff_block]
  ct_b:
    base: input1
    layout: [component, q_limb, coeff_block]
  rlk:
    base: eval_key
    layout: [digit, rlk_component, basis, coeff_block]
  output:
    base: output
    layout: [component, q_limb, coeff_block]
```

---

## 5. 指令语义与合法化规则

LLVM/MLIR 后端通常需要 instruction selection 和 legality 信息。HPU 也需要为每条助记符提供完整语义。

每条指令至少需要提供：

| 字段 | 必需性 | 说明 |
| --- | --- | --- |
| `mnemonic` | 必需 | 汇编助记符 |
| `format` | 必需 | AR3/STG/CFG/SYNC/custom1 等 |
| `operands` | 必需 | 操作数类型、位宽、取值范围 |
| `encoding` | 必需 | 每个字段对应机器码 bit range |
| `semantics` | 必需 | 数学语义 |
| `modulus_context_required` | 必需 | 是否依赖 `pmodld` |
| `shuffle_context_required` | 必需 | 是否依赖 `pshcfg` |
| `side_effects` | 必需 | 是否读写 SRAM、外存、上下文寄存器 |
| `latency` | 建议 | 指令延迟 |
| `throughput` | 建议 | 吞吐 |
| `pipeline` | 建议 | 使用哪个执行管线 |
| `hazards` | 必需 | RAW/WAR/WAW、上下文切换危险 |
| `undefined_behavior` | 必需 | 非法 operand 或上下文缺失时行为 |

示例：

```yaml
instruction:
  mnemonic: pntt
  format: STG
  operands:
    pdata: pobj
    ptwiddle: pobj
    stage: imm4
    idx1: imm
    mode: imm
  reads: [pdata, ptwiddle, mod_ctx]
  writes: [pdata]
  semantics: in_place_ntt_stage
  requires:
    - pmodld before use
    - twiddle object matches stage
```

---

## 6. 上下文状态模型

HPU 有隐式状态，例如当前模上下文、shuffle 配置、随机种子等。编译器必须知道这些状态如何影响指令。

| 状态 | 必需信息 |
| --- | --- |
| `mod_ctx` | 容量、格式、`pmodld` index 语义、切换延迟、跨指令保持规则 |
| `shuffle_cfg` | 格式、`pshcfg` index 语义、适用指令 |
| `seed` | `pseed` 生效范围、随机流确定性、并发规则 |
| `pipeline_state` | `psync` 是否清空 pipeline、是否等待 DMA |
| `fault_state` | 错误如何上报给 host |

必须明确：

```text
pmodld 后第几条指令开始可见
pmodld 是否影响所有后续算术指令
pntt/pintt 是否读取当前 mod_ctx
psync 是否保证 dstore 已对外部内存可见
```

---

## 7. 调度、流水线与同步约束

编译器调度器需要硬件给出真实的依赖和代价模型。

| 字段 | 必需性 | 说明 |
| --- | --- | --- |
| `issue_width` | 必需 | 每周期可发射指令数 |
| `pipeline_classes` | 必需 | DMA、AR3、NTT、CFG、SYNC 等管线 |
| `latency_table` | 建议 | 每条指令延迟 |
| `resource_table` | 建议 | 每条指令占用资源 |
| `barrier_semantics` | 必需 | `psync` 等待哪些资源 |
| `dma_compute_overlap` | 必需 | DMA 与计算能否重叠 |
| `context_switch_penalty` | 建议 | `pmodld/pshcfg` 切换代价 |
| `bank_conflict_rules` | 建议 | SRAM bank 冲突规则 |

需要特别声明的 hazard：

```text
dload 写 p0 后，p0 何时可被 pmul 读取
pmul 写 p2 后，p2 何时可被 dstore 读取
pmodld 切换 q_i 后，下一条 pmul 是否立即使用新模数
连续 dstore rel=1 后，槽位是否可立即 dload 覆盖
```

---

## 8. 数据布局规范

为了让软件 reference、测试数据和硬件行为一致，所有外部对象都需要标准布局。

必须定义的对象：

| 对象 | 必须给出布局 |
| --- | --- |
| ciphertext | `[component][q_limb][coeff]` 或其他明确顺序 |
| plaintext | `[q_limb][coeff]` |
| tensor product | `[component=0..2][q_limb][coeff]` |
| relin/eval key | `[digit][key_component][basis][coeff]` |
| mod context | 每个 prime 的 modulus、inverse、Barrett/Montgomery 参数 |
| twiddle | `[basis][stage][entry]` 或硬件要求格式 |
| bconv constants | `qhat_inv`, `qhat_modp`, `phat_inv`, `phat_modq` |
| temporary buffers | ModUp、ModDown、correction term、accumulator |

每个布局必须声明：

```text
元素类型
字节序
对齐
padding
component 顺序
RNS basis 顺序
NTT order
是否 Montgomery form
是否 lazy reduction
```

---

## 9. FHE 算子语义 Contract

后端不只需要单条指令，还需要算子级 contract。否则 `cmult`、`keyswitch`、`ciphertext_multiply` 很容易在 domain 或 layout 上失配。

每个算子建议声明：

| 字段 | 说明 |
| --- | --- |
| `name` | 算子名 |
| `input_domain` | coefficient / NTT |
| `output_domain` | coefficient / NTT |
| `input_layout` | 外存布局 |
| `output_layout` | 外存布局 |
| `required_constants` | twiddle、mod ctx、bconv 常量、rlk 等 |
| `temporary_buffers` | 所需临时空间 |
| `valid_params` | `N/num_q/num_p/dnum` 约束 |
| `side_effects` | 修改哪些输出或 scratch |
| `sync_required` | kernel 末尾是否需要 `psync` |

以完整密文乘法为例：

```yaml
operator:
  name: ciphertext_multiply
  inputs:
    ct_a: {components: 2, domain: coefficient, basis: Q}
    ct_b: {components: 2, domain: coefficient, basis: Q}
    rlk:  {components: 2, domain: ntt, basis: QP, layout: digit-major}
  outputs:
    ct_out: {components: 2, domain: coefficient, basis: Q}
  steps:
    - NTT(ct_a, ct_b)
    - tensor_product -> t0,t1,t2
    - INTT(t0,t1,t2)
    - keyswitch(t2, rlk) -> ks0,ks1
    - add(t0,ks0), add(t1,ks1)
```

---

## 10. 编码、汇编与反汇编接口

如果后续要接 LLVM/MLIR，需要把汇编和机器码能力变成稳定接口。

| 能力 | 必需性 | 说明 |
| --- | --- | --- |
| assembler grammar | 必需 | 合法助记符、操作数、注释、换行规则 |
| binary encoding | 必需 | bit-level 编码 |
| disassembler | 建议 | `.inst32` 反解为 asm，方便调试 |
| verifier | 必需 | 检查 operand range、slot range、stage range |
| feature gating | 必需 | 不同 ISA 版本允许的指令集合 |
| diagnostic format | 建议 | 错误位置、错误原因、建议修复 |

---

## 11. Cost Model 与优化信息

编译器想做 tiling、fusion、layout selection、prefetch，需要硬件提供成本模型。

| 信息 | 用途 |
| --- | --- |
| 每条指令 latency/throughput | 指令调度 |
| NTT 每 stage 代价 | NTT fusion 和跨算子优化 |
| DMA 带宽和启动开销 | 预取与分块 |
| `pmodld` 代价 | RNS loop ordering |
| `dstore rel=1` 代价 | 槽位复用策略 |
| SRAM 容量和 bank 冲突 | tile 大小 |
| Q/P basis 切换代价 | ModUp/ModDown 排序 |
| fused 指令收益 | pattern rewrite |

---

## 12. 验证与测试向量要求

每个 target 版本必须随硬件约束一起提供测试向量。

最低要求：

| 测试 | 内容 |
| --- | --- |
| instruction encoding test | asm 到 inst32 的 golden |
| instruction semantic test | 单条 `padd/pmul/pntt/dload` 行为 |
| layout roundtrip test | 外存对象加载、写回、反序列化一致 |
| NTT/INTT test | 小 N 和真实 N 的 roundtrip |
| BConv test | Q->P、P->Q golden |
| KeySwitch test | digit decomposition + modup + evk + moddown |
| CiphertextMultiply test | 完整乘法含重线性化 |
| stress test | 最大 `N/num_q/num_p/dnum` |

测试向量目录建议：

```text
outputs/<case>/test_data/
  params.json
  input.bin
  constants.bin
  expected_intermediate/
  expected_output.bin
  memory_descriptor.json
```

---

## 13. 推荐交付物清单

硬件团队或后端维护者至少需要交付以下文件：

```text
target/
  hpu_target.yaml              # 全局 target description
  hpu_isa.yaml                 # 指令格式、语义、编码、合法范围
  hpu_memory_abi.yaml          # DMA、descriptor、布局、地址公式
  hpu_operator_contracts.yaml  # NTT/CMULT/KeySwitch/CiphertextMultiply 等算子语义
  hpu_cost_model.yaml          # latency、throughput、资源占用
  hpu_verifier_rules.yaml      # 静态检查规则
  tests/
    encoding/
    semantics/
    operators/
```

这些文件可以进一步生成：

```text
LLVM TableGen definitions
MLIR dialect attributes/types/ops
C++ TargetInfo
汇编 parser verifier
软件 reference 测试配置
```

---

## 14. 当前项目最急需补齐的信息

结合现有代码，优先级最高的是：

1. **DMA 地址 ABI**
   明确 `dload x0, x0, p?, type` 到底如何选择 ct、rlk、twiddle、mod ctx、临时 buffer。

2. **外存布局**
   明确密文、三元张量积、重线性化密钥、ModUp/ModDown 中间值的排列。

3. **域约定**
   明确每个算子输入输出是 coefficient domain 还是 NTT domain。

4. **槽位生命周期**
   明确 `dstore rel=1` 是否真的释放对象，以及释放后何时可被 `dload` 复用。

5. **上下文可见性**
   明确 `pmodld`、`pshcfg` 后续指令何时能看到新上下文。

6. **同步语义**
   明确 `psync` 是否等待 DMA 完成，以及是否保证 `dstore` 对 host 可见。

7. **重线性化密钥布局**
   明确 `rlk[digit][component][basis]` 的 basis 顺序、domain 和地址步长。

8. **golden test**
   提供小参数和真实参数下的完整 `ciphertext_multiply` reference 数据。

---

## 15. 和 LLVM/MLIR 对接时的映射建议

| HPU 信息 | LLVM/MLIR 类比 |
| --- | --- |
| ISA 指令、编码 | LLVM TableGen `InstrInfo` |
| 对象槽位 | Register class / custom resource |
| `pmodld/pshcfg` 隐式状态 | MachineFunction state 或 MLIR op attributes |
| 数据布局 | MLIR type/layout attribute |
| DMA descriptor | ABI lowering / calling convention |
| 算子 contract | MLIR dialect op verifier |
| 参数合法性 | Type conversion + legalization |
| cost model | TargetTransformInfo / MLIR cost model |
| `.inst32` 编码 | MC layer assembler backend |

推荐分层：

```text
FHE Dialect
  -> HPU Operator Dialect
  -> HPU Machine Dialect
  -> HPU ASM
  -> HPU inst32
```

这样 `ciphertext_multiply` 可以先作为高层 op 存在，再逐步 lowering 到：

```text
NTT
CMULT tensor product
INTT
KeySwitch / Relinearization
Final Add
DMA / slot-level instructions
```

