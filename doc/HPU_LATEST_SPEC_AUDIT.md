# HPU 最新文档符合性审计

审计日期：2026-07-23

## 1. 审计基线

本审计以以下飞书文档为依据：

1. [HPU 控制逻辑设计文档](https://icnj64z5e8zz.feishu.cn/wiki/KOlSwfEEtiMuqvkTppPcyJElnyf)，v0.4，2026-05-31。按项目负责人确认，作为当前 26-bit 命令、对象状态、allocator、small Bank 5、`pmodld`、`pfree` 和 `psync` 的首要基线。
2. [HPU 集成与编程手册](https://icnj64z5e8zz.feishu.cn/wiki/NZEgwsvshiQ6Twkrxvtck3UGnXg)，V0.2，2026-07-10。用于跨模块接口、CSR、SRAM、DMA 和软件流程；与控制逻辑文档冲突时不覆盖上述控制逻辑口径。
3. [RISC-V核内接口设计](https://icnj64z5e8zz.feishu.cn/wiki/QE8MwYGIciNwoYkjomCcZbnmnOh)。核内 custom0/custom1 发射路径基线；其中 custom1 章节与集成手册存在冲突，见第 4 节。
4. [HPU_PE_反串讲](https://icnj64z5e8zz.feishu.cn/wiki/T7pTwV4eiiJbXHkTkrAcgDzxn0g)，v0.1，2026-06-22。PE 位宽、Barrett、twiddle 和 NTT/INTT 数据通路验证基线。
5. [HPU](https://icnj64z5e8zz.feishu.cn/wiki/MZkHwbivGiOs7ekMGb0cMSk5nTY)。知识库根文档，包含新旧章节，只用于定位历史约定，不作为单一冻结版本。
6. [HPU 通过 DMA 访问主存的实现方案讨论稿](https://icnj64z5e8zz.feishu.cn/wiki/KOfhwRW4Oi33f6kPEXWcJJwDnSS)。该文档明确是讨论稿；当它与集成手册冲突时，以集成手册为准。

## 2. 已完成修复

### A0. `pfree` / `psync` OPC 反置

- 文档来源：《HPU 集成与编程手册》3.1.3。
- 文档约定：`PSYNC=0111`，`PFREE=1000`。
- 原项目：`PFREE=0111`，`PSYNC=1000`。
- 当前状态：已修复编码器、编码单测、delivery 检查和项目指令手册。
- 当前 RV 编码示例：`pfree p5 = 0x8140000B`，`psync = 0x7000000B`。

### A1. custom0 与 26-bit precode 字段契约（已修复，2026-07-23）

冻结映射为：

```text
cmd26[25]   = custom_kind (0=custom0, 1=custom1)
cmd26[24:0] = control payload
```

当前实现已经：

1. 将 STG 的 stage 移到原始指令 `[13:10]`，mode/flag 分别使用 `[9:8]`、`[7]`。
2. 将 AR3 立即数模式移动到 2-bit MODE 的 `[9:8]`。
3. 增加 `precode_command26()`，所有可编码算子同时输出 `.inst32` 和 `.cmd26`。
4. custom1 precode 将原始指令的 `flag/OBJ_ID/TYPE/DIR` 重排到控制 payload，`rs1/rs2` 作为 line offset/count sideband。
5. 生成 `expected_cmd26.csv`，逐条记录源 payload、控制 payload、custom kind 和最终命令。

## 3. 项目与最新文档的差异

### A2. `pmodld` 旧“模上下文对象”语义（已修复，2026-07-21）

文档来源：《HPU 控制逻辑设计文档》“PMODLD 指令详细位段”和 `hpu_cfg_state_regs` 章节；《HPU 集成与编程手册》3.1.3、3.5.4、3.6.1。

当前实现已经完成以下迁移：

1. 汇编语法改为 `pmodld mod_id`，范围 0..255；旧 `psrc/idx1/cfg15` 语法作为负例拒绝。
2. 原始 32-bit 指令的 `MOD_ID` 编码在 `[21:14]`，经过 custom0 precode 后对应 `cmd26[14:7]`。
3. 所有算子生成器均改为 `pmodld(i)`，不再把 `p4` 编入 `pmodld`。
4. `MOD_ID` 编码仍为 8-bit；根据 `SMALL_BANK_LINES=8`、每 line 16 context，生成器物理上限为 128。对象槽位仍独立保持 8 个。

`dload type=2, flag[0]=1` 现在显式请求 allocator 将模表对象分配到 small Bank 5；生成器在首条 `pmodld` 前插入 `psync`。模表对象是具有 `ALLOC/V/busy/base/len` 的真实逻辑对象，不再描述为“仅 DMA 句柄”。剩余接口项是 `mod_table_base_line` 与该对象实际 `base` 的绑定。

### A3. `pfree` 对象字段和 `psync` 载荷（已修复，2026-07-23）

`pfree` 对象已移动到原始 custom0 `PSRC/OBJ_ID=[24:22]`；其他载荷位为 0。`psync` 语法改为无操作数，所有载荷位为 0，并按控制逻辑统一 inflight 屏障使用。

### A4. 当前 `.inst32` 不是硬件可执行 DMA 流（P0）

本地证据：`src/operator/ciphertext_multiply.cpp` 和各算子生成器全部输出 `dload/dstore x0, x0`；`output/ciphertext_multiply.asm` 因而没有真实 line offset/count。

文档来源：《HPU 集成与编程手册》3.3、5.2.2、9.1。`rs1/rs2` sideband 应给出 256B line offset 和非零 line count；长度为 0 或越界会触发 fault。

修改建议：

1. 增加指令流 relocation/scheduling 阶段，读取 `line_map.csv` 并为每条 DMA 指令绑定 offset/count。
2. 输出可直接运行的 host harness：装载寄存器、发射 `.inst32`、等待完成、检查 fault。
3. 将“所有 DMA 使用 x0/x0”改成 delivery 失败条件；仅显式 `--symbolic-dma` 模式允许占位符。

### A5. stage twiddle 物理布局与 PE 文档不一致（P0）

本地证据：`test/reference/main.cpp` 为 stage `s` 只生成 `2^s` 个唯一 twiddle，并声明由 butterfly group 复用。当前 NTT 指令流也没有消费单独生成的 `pre_twist` 和 `post_untwist_scale` 对象。

文档来源：《HPU_PE_反串讲》13.2、13.3：每个 stage 的物理 twiddle 对象为 `N/2` 个 32-bit 元素；以 `N=65536` 为例正好是 512 line。每个 stage 前独立 DLoad。

影响：数学 reference 的 twiddle 正确，不代表 DMA 后的 PE 物理 lane 顺序正确。对当前 `N=4096`，文档口径应为每 stage 2048 words/32 lines，而项目 stage 0..10 均更短。

现有 `STAGE_TWIDDLE_LAYOUT=PASS` 只检查当前项目的压缩布局和 manifest 自洽，不能作为满足上述 PE 物理布局的证明。

修改建议：由 PE/stream_ctrl 负责人给出每个 stage 的 lane 读取顺序，按该顺序展开为 `N/2` 元素；每个 stage 固定 `N/128` line。另需明确 negacyclic pre/post factor 是由硬件隐式完成，还是由软件额外发出 PMUL/NTT 阶段。

### A6. HPU_MEM CSR 已有数字地址，但项目仍标记“待确认”（P1）

本地证据：`test/reference/main.cpp` 生成的 `hpu_mem_config.json` 仍写 `RTL_CONFIRM_REQUIRED`，使用 `*_SHADOW`、合并的 `SIZE_LINES_SHADOW` 和 `HPU_MEM_STATUS` 名称；`memory_map.json`、README 和交付文档仍把数字偏移列为 pending。

文档来源：《HPU 集成与编程手册》5.2.3 表 5.6。

最新映射为：

| offset | register |
| --- | --- |
| `0x00` | `HPU_MEM_BASE_LO` |
| `0x04` | `HPU_MEM_BASE_HI` |
| `0x08` | `HPU_MEM_SIZE_LINES_LO` |
| `0x0C` | `HPU_MEM_SIZE_LINES_HI` |
| `0x10` | `HPU_MEM_COMMIT` |
| `0x14` | `HPU_STATUS` |
| `0x18` | `HPU_FAULT_STATUS` |

修改建议：在 JSON 中输出数字 offset、读写属性和值，拆分 size low/high；更新 delivery 检查，删除该 pending 项。

### A7. 模上下文镜像字节可兼容，但元数据和校验未按 48-bit mu 描述（P1）

本地证据：当前镜像是四个 word：`q, mu_lo, mu_hi, reserved`；只检查 `modulus > 1`。

文档来源：《HPU 集成与编程手册》3.1.2、3.5.4；《HPU_PE_反串讲》6.3。PE 有效 `mu` 为 48-bit，且模数必须满足 `65537 <= q <= 2^32-1`。

由于合法 q 下 `floor(2^64/q)` 的高 16 位恒为 0，当前四个 word 的实际字节可与 `{reserved[47:0], mu[47:0], q[31:0]}` 兼容。仍应把 ABI 文案改成 `q32 + mu48 + reserved48`，并断言 `mu >> 48 == 0` 和 q 范围，避免以后换参数时静默生成 RTL 不支持的数据。

### A8. 参数检查没有覆盖本地 SRAM/PE 能力（P1）

本地证据：NTT 和完整乘法仍主要检查 N 为 2 的幂。context 数已按 Bank 5 的 8 line 容量限制为 128，但尚未统一检查本地 SRAM 峰值驻留等目标能力。

文档来源：《HPU 集成与编程手册》3.1.2、3.4；《HPU 控制逻辑设计文档》allocator 章节。

修改建议：增加统一 `HpuTargetConfig` 校验，至少检查 `ceil(N/64) <= 1024`、完整 line 要求、twiddle `N/2` 的 line 数、q/mu 范围、`MOD_ID` 范围、8 个并发对象和 bank/half-bank 峰值驻留。对象数与 RNS limb/context 数必须分开建模。

### A9. PE golden 是数学结果，不是位精确硬件 reference（P1）

本地证据：`test/reference/main.cpp::mul_mod` 使用 128-bit 乘法后直接 `% modulus`。

文档来源：《HPU_PE_反串讲》6.3 和 14 章。该文档要求 reference 按 48-bit mu、33-bit `q_hat`、33-bit 低位乘法和一次修正建模，并覆盖 Barrett 修正分支。

修改建议：保留现有数学 golden，再增加独立 PE bit-exact model 和 corner vectors。两者结果应相同，但 bit-exact model 用于定位截断、流水线和边界实现错误。

### A10. runtime 缺少 non-coherent 与故障协议（P1）

本地证据：当前交付只有指令、镜像和配置 JSON，没有 cache clean/invalidate、CSR fault 清除和中断等待代码。

文档来源：《HPU 集成与编程手册》5.2、5.2.8、9.1。

修改建议：提供最小 runtime/driver 层，完成 HPU_MEM ownership、提交前 clean、读回前 invalidate、`HPU_STATUS/FAULT_STATUS` 检查、W1C fault、`irq_sync_done` 和 DMA 错误处理。

### A11. NTT/INTT 的“原地”描述过强（已修复，2026-07-23）

本地证据：项目手册和 `src/util/ntt.cpp` 声明 NTT/INTT 原地执行。

文档来源：《HPU 控制逻辑设计文档》描述 NTT/INTT out-of-place 分配并在完成后提交新 base；《HPU_PE_反串讲》13.6 又记录设计版本存在 in-place/out-of-place 差异。

当前软件 ABI 已统一表述为“同一 logical object id，物理 base 可由 controller 在 stage 完成后重分配并提交”，不再承诺物理原地。PE 文档中的版本差异仍需由 RTL release note 记录，但不会改变软件对象号。

## 4. 飞书文档之间需要硬件负责人确认的矛盾

这些项目不能直接由软件仓库单方面决定：

| ID | 矛盾 | 来源文档 | 建议冻结口径 |
| --- | --- | --- | --- |
| C1 | `psync` 是否等待 custom1/DMA | 按当前首要基线《HPU 控制逻辑设计文档》，统一 inflight 完成包含 `extmem_done_pulse` | 已按包含 DMA 完成实现；模表 dload 后显式插入 `psync` |
| C2 | custom1 是 rs1/rs2 line sideband，还是 VA 经 DTLB 后形成 `{paddr,len,dir,flags}` descriptor | 《HPU 集成与编程手册》5.2.2/9.1 与《RISC-V核内接口设计》custom1 HpuUnit 章节相反 | 按用户决定，保留项目现有 `rs1/rs2` 32-bit 位域，仅在原保留 `inst[8]` 增加 `flag[0]`；precode 后按控制逻辑命令格式消费 |
| C3 | 模上下文记录是 `mu64+reserved32` 还是 `mu48+reserved48` | 《HPU 控制逻辑设计文档》写 `{reserved[31:0],mu[63:0],q[31:0]}`；《HPU 集成与编程手册》3.5.4 写 `{reserved[47:0],mu[47:0],q[31:0]}` | 以 PE 实际 48-bit 端口为准；合法 q 下两种全零高位镜像字节兼容，但文档统一为 mu48 |
| C4 | NTT/INTT 物理 in-place 或 out-of-place | 《HPU 控制逻辑设计文档》为 out-of-place；《HPU_PE_反串讲》13.6 记录“设计改了 in-place / 分 stage 混合”的未决说明 | 软件只承诺 logical object id；RTL release note 给出物理策略 |
| C5 | 32-bit 原始指令与 26-bit 内部命令映射 | 《HPU 控制逻辑设计文档》v0.4 规定 `cmd[25]=cmd_kind`；《RISC-V核内接口设计》只确认核侧可提取 custom0 的 `inst[31:7]`，其中后续 custom1 descriptor 方案与控制逻辑的统一命令入口不同 | 按项目负责人最新确认，以《HPU 控制逻辑设计文档》为准：kind 位于最高位，custom1 由 precode 重排语义字段并携带独立 sideband |

## 5. 建议实施顺序

1. 冻结仍未解决的 C2-C4，并形成带版本号的 machine-readable target ABI；C1/C5 已冻结。
2. A1/A2/A3 已完成，持续用 RV smoke 的 32/26-bit 期望表回归。
3. 完成 A4、A6、A10：生成真实 DMA sideband、CSR 程序和可运行 host harness。
4. 完成 A5、A7-A9：重做物理 twiddle、目标参数校验和 PE bit-exact UT。
5. 将 `.inst32 + cmd26 + HPU_MEM image + CSR sequence + expected checkpoints` 一起接入 RTL IT；只有该流程通过后，才能把 `HARDWARE_EXECUTION` 从 `CONDITIONAL` 改为 `PASS`。
