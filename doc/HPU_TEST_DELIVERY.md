# HPU 指令流与验证数据交付说明

## 1. 交付结论

本仓库已形成可复现的软件交付闭环：生成 HPU 算子指令流、编码为 32-bit 指令、生成完整密文乘法及重线性化的 RNS golden 数据、执行软件解密校验，并生成 RV 接口冒烟用例。

一键生成和验收：

```bash
cmake -S . -B build
cmake --build build -j --target hpu_delivery
ctest --test-dir build --output-on-failure
```

成功后，`outputs/DELIVERY_REPORT.txt` 中应包含：

```text
SOFTWARE_DELIVERY=PASS
FHE_REFERENCE=PASS
ASM_ENCODING=PASS
RV_INTERFACE_SMOKE=PASS
OPERATOR_UT_PACKAGES=PASS
HARDWARE_EXECUTION=CONDITIONAL
```

`HARDWARE_EXECUTION=CONDITIONAL` 不是算法失败，而是提醒联调人员：RTL 尚需确认 DMA 地址 ABI、`mod_ctx`/twiddle 打包、scratch 地址和 DMA 完成语义。确认这些字段前，当前 `.inst32` 中的 `dload/dstore x0,x0` 只能作为计算顺序流，不能直接当成最终可运行程序。

## 2. 程序入口与执行顺序

项目包含三个独立可执行入口，不存在 `test/reference/main.cpp` 替代 `src/main.cpp` 的关系：

| 顺序 | 源入口 | 可执行文件 | 职责 |
| --- | --- | --- | --- |
| 1 | `src/main.cpp` | `inline_asm_codegen` | 生成 `output/*.cpp` 和 `output/*.asm` 指令流 |
| 2 | `test/encode/main.cpp` | `inline_asm_encode_outputs` | 归档到 `outputs/`、编码 `.inst32`、生成 RV 冒烟流 |
| 3 | `test/reference/main.cpp` | `hpu_reference_vectors` | 生成并校验完整乘法 golden，拆分各算子 UT 数据 |

顶层 `hpu_delivery` 依次执行三个程序，再调用 `test/delivery/check_delivery.cmake` 检查文件完整性、FHE 校验结果、阶段标记和指令数量。单独执行 `hpu_reference_vectors` 只会生成数据，不会生成或更新 HPU ASM。

当前存在两组需要保持一致的源配置：

- `src/main.cpp`：HPU 指令生成参数，完整乘法和 KeySwitch 复用 `kCiphertextMultiplyCfg`。
- `test/reference/main.cpp`：软件 reference 参数 `kN/kNumQ/kNumP/kDnum/kPlainModulus/kSeed`。

`outputs/*/test_data/params.json` 是 reference 写出的结果清单，不是配置入口。修改它不会影响生成逻辑，并会在下一次执行 `hpu_delivery` 时被覆盖。

## 3. FHE 算法流程

Golden 严格执行以下顺序：

```text
Encrypt(ctA, ctB)
  -> NTT(ctA[0], ctA[1], ctB[0], ctB[1]) over Q
  -> TensorProduct(t0, t1, t2) over Q
  -> INTT(t0, t1, t2)
  -> Decompose t2 into Q digits
  -> Hybrid ModUp each digit to full Q union P
  -> NTT over Q union P
  -> Multiply-accumulate with rlk[digit][0..1]
  -> INTT over Q union P
  -> ModDown P from both key-switch components
  -> (out0, out1) = (t0 + ks0, t1 + ks1)
  -> Decrypt and compare with mA * mB in Z_t[x]/(x^N+1)
```

当前参数与主指令流一致：`N=4096`、`num_q=4`、`num_p=3`、`dnum=2`。数据使用确定性、无噪声、P 可整除的功能测试评估密钥，以获得逐位可比结果；它用于 UT/IT 定位，不代表生产密钥安全性。

## 4. 数据格式

完整乘法数据位于 `outputs/ciphertext_multiply/test_data/`：

| 文件 | 用途 |
| --- | --- |
| `params.json` | 模数、根、环参数、NTT 约定和安全属性 |
| `artifact_manifest.csv` | 每个二进制及可读文本的路径、shape、字节数和 FNV-1a 校验值 |
| `memory_map.json` | 建议的逻辑 DDR 地址映射及硬件待确认字段 |
| `dma_plan.csv` | 各算法阶段的输入、输出、域和基顺序 |
| `input/*.bin` | 两个输入密文、测试明文和测试私钥 |
| `constants/*.bin` | `rlk[digit][component][basis][coefficient]` |
| `expected/*.bin` | NTT、tensor、ModUp、KeySwitch、ModDown 和最终结果检查点 |
| `VALIDATION.txt` | 软件参考模型最终校验结果 |

所有 `.bin` 均采用 little-endian `uint64_t` canonical residue。多维数组按 C row-major 展平，最后一维始终是 coefficient；基顺序固定为 `Q[0..3]` 后接 `P[0..2]`。

每个 `.bin` 都有同名 `.hex.txt` 人工可读版本，例如 `input.bin` 对应 `input.hex.txt`。文本文件头包含用途、shape、维度含义和编码说明，多维数据按 component/digit/basis 分块，并在每行标注 coefficient 范围。

## 5. 失败定位

| 首个失败检查点 | 优先排查模块 |
| --- | --- |
| `inputs_ntt_q` | NTT、twiddle、模上下文、数据排列 |
| `tensor_ntt_q` | PE 的 `pmul/pmac`、活动模上下文 |
| `tensor_coeff_q` | INTT、归一化因子、输出排列 |
| `modup_t2_coeff_qp` | BConv 常数、digit 偏移、Q/P context 编号 |
| `keyswitch_accum_ntt_qp` | rlk 布局、digit/component/basis 步长、PMAC 累加 |
| `keyswitch_moddown_q` | P->Q BConv、`P^-1 mod q_i`、减法方向 |
| `ciphertext_out_q` | 最终 `padd`、输出 component 顺序 |
| 最终解密 | 上述节点均通过时再检查方案参数和 host 数据解释 |

同一 reference 还会拆分到 `outputs/{ntt,intt,mm,bconv,modup,pmult,cmult,moddown,keyswitch}/test_data/`。每个目录均包含独立 `params.json`、输入、期望输出和带 checksum 的 `artifact_manifest.csv`，可直接交给对应模块负责人跑 UT。`auto` 当前无法完成物理寄存器编码，其阻塞原因记录在 `outputs/auto/test_data/STATUS.md`。

## 6. RV 接口用例

`outputs/rv_interface_smoke/` 包含：

- `rv_interface_smoke.asm`：覆盖全部指令类别、四种 DLoad type、DStore retain/release 和最大合法字段。
- `rv_interface_smoke.inst32`：对应 32-bit 指令流。
- `test_data/expected_decode.csv`：逐条期望 word、`custom0/custom1` 路由和归一化汇编。
- `test_data/negative_cases.asm.txt`：必须被译码器/验证器拒绝的越界用例。

建议 RV 接口 IT 依次验证 decode 路由、队列 backpressure、顺序发射、`pmodld -> compute` 可见性、`dload -> compute -> dstore` ownership，以及 `psync` 中断边界。

## 7. 硬件联调前置确认

以下五项必须由控制逻辑、SRAM、DMA 和软件负责人共同签字确认，确认后才能把当前交付从“软件可验收”升级为“硬件可直接运行”：

1. `rs1` 地址和 `rs2` 长度的单位、对齐、最大值及跨 4 KiB 行为。
2. `mod_ctx` 中模数、约减常数、NTT 根和逆元的精确位布局。
3. 每个 `pntt/pintt stage` 消费的 twiddle 次序及 Montgomery 域约定。
4. ct、tensor、ModUp、rlk、KeySwitch scratch 的物理地址和生命周期。
5. DMA 完成、对象可用、`dstore rel` 与 `psync` 中断之间的 happens-before 关系。
