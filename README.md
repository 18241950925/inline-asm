# HPU Inline Assembly Codegen

本项目用于为全同态加密在自研 HPU（Homomorphic Processing Unit）硬件上，提供 C++ 内联汇编（Inline Assembly）的生成器。项目基于 HPU 的 3-bit 寻址槽位和流水线执行语义进行抽象，支持从底层通用计算直至高层复杂 FHE 算子的自动代码生成，并可将生成后的 ASM 继续转译为 32 位指令编码文本。

## 1. 项目结构

代码按照功能依赖分层，并分别维护在 `include` 及 `src` 目录下，包含三大类生成模块，以及独立的编码、reference 与测试辅助模块：

### 1) 基础工具层 (`util`)
- **`util/hpu_asm.hpp/cpp`**：基础 HPU 汇编助记符封装和生成接口，遵循《HPU_INSTRUCTION_MANUAL.md》。
- **`util/ntt.hpp/cpp`**：按 stage 推进的基于对象槽位语义的 NTT / INTT 汇编生成。
- **`util/mm.hpp/cpp`**：对象槽位级别的四则运算，特别是逐点向量乘法、乘加积累等（`pmul` / `pmac`）。
- **`util/bconv.hpp/cpp`**：对象槽位上带参数 `q_offset` 支持分组扩展的基础 Basis Conversion 两阶段汇编生成。

### 2) 多项式级操作层 (`poly`)
调用并复用 `util` 层的基础算子，组合出同态相关的复杂多项式算子：
- **`poly/pmult.hpp/cpp`**：明密文相乘 (Plaintext-Ciphertext Multiplication)。
- **`poly/cmult.hpp/cpp`**：密文乘法的张量积阶段，将二元密文 $(a_0,a_1)$ 与 $(b_0,b_1)$ 乘成三元中间结果 $(t_0,t_1,t_2)$。
- **`poly/modup.hpp/cpp`**：模提升 (ModUp) 操作，负责将 $Q$ 基下的残差提升扩展到 $Q \cup P$。
- **`poly/moddown.hpp/cpp`**：模回缩 (ModDown) 操作，负责将中间结果缩放回 $Q$ 基，纠正缩放因子。

### 3) 高级算子层 (`operator`)
拼装多项式级与基础工具算子，完整实现核心同态运算：
- **`operator/keyswitch.hpp/cpp`**：完整密钥切换 (KeySwitch) 逻辑生成，包含切片循环（密文分解，由参数 `dnum` 控制）、ModUp、多基 NTT、与 Evk (评估密钥) 点乘累加、INTT、以及 ModDown 缩放累加操作的全工作流流水线。
- **`operator/ciphertext_multiply.hpp/cpp`**：完整密文乘法生成，执行输入分量 NTT、`cmult` 三分量张量积、INTT、对 $t_2$ 的重线性化/KeySwitch，以及最终 $(out_0,out_1)$ 合成。

### 4) 指令编码模块 (`encode`)
将生成出的 HPU 汇编进一步转译为 32 位机器码文本：
- **`encode/include/*.hpp`**：定义指令数据结构、解析、编码及组装接口。
- **`encode/src/*.cpp`**：实现 ASM / C++ 内联汇编解析、格式归一化以及 `custom0` / `custom1` 指令编码。
- **`hpu_encode`**：由 `encode/CMakeLists.txt` 生成的静态库，供后续测试或上层流程复用。

### 5) 编码测试辅助模块 (`test/encode`)
用于把主生成流程输出的 ASM 继续转换为 `.inst32` 文件：
- **`test/encode/main.cpp`**：读取主流程生成的 `output/<case>.cpp` 与 `output/<case>.asm`，归档到 `outputs/<case>/`，再调用 `hpu_encode` 生成对应的 32 位二进制文本。
- **`inline_asm_encode_outputs`**：构建后生成的测试编码工具。

### 6) 软件 Reference (`test/reference`)
- **`test/reference/main.cpp`**：独立的软件算法入口，生成确定性的 RNS/RLWE 输入、重线性化密钥、完整密文乘法 golden、中间检查点和各算子 UT 数据，并执行解密一致性校验。
- **`hpu_reference_vectors`**：构建后生成的 reference 数据工具；它不生成 HPU 指令，也不替代 `src/main.cpp`。

### 7) 项目文档 (`doc`)
- **`doc/HPU_INSTRUCTION_MANUAL.md`**：当前 HPU 指令格式和语义说明。
- **`doc/HPU_TEST_DELIVERY.md`**：指令流、完整密文乘法 golden、RV 接口冒烟用例、验收命令和硬件联调前置项。

### 8) 三个程序入口

| 可执行文件 | 源入口 | 职责 | 主要输出 |
| --- | --- | --- | --- |
| `inline_asm_codegen` | `src/main.cpp` | 调用各级 codegen，生成 HPU C++ 内联汇编和 ASM body | `output/*.cpp`、`output/*.asm` |
| `inline_asm_encode_outputs` | `test/encode/main.cpp` | 归档生成结果、编码 `.inst32`、生成 RV 接口冒烟流 | `outputs/<case>/*`、`outputs/rv_interface_smoke/*` |
| `hpu_reference_vectors` | `test/reference/main.cpp` | 计算软件 golden、解密校验并拆分 UT/IT 数据包 | `outputs/<case>/test_data/*` |

`src/main.cpp` 仍是指令生成主入口。`test/reference/main.cpp` 是另一独立可执行文件的入口，两者没有替代关系。顶层 `hpu_delivery` 目标只是按上述顺序编排三个程序并运行交付检查。

---

## 2. 一键构建与运行

```bash
mkdir -p build
cd build
cmake ..
cmake --build . -j
```
构建完成后，生成的可执行文件 `inline_asm_codegen` 支持通过命令行参数控制输出内容：
```bash
# 只输出 .asm 文件
./build/inline_asm_codegen asm

# 只输出 .cpp 文件
./build/inline_asm_codegen cpp

# 两者皆输出 (默认行为，等价于不传参)
./build/inline_asm_codegen both
```

若需要继续将可编码的 ASM 转成 32 位指令文本，可在生成 ASM 后执行：

```bash
./build/test/encode/inline_asm_encode_outputs
```

推荐使用统一交付目标，一次完成指令生成、编码、FHE reference 数据生成和交付门禁检查：

```bash
cmake --build build -j --target hpu_delivery
ctest --test-dir build --output-on-failure
```

主生成程序仍保持主分支的输出方式，先在根目录生成扁平的 `output/` 文件夹；编码辅助工具随后会把 `.cpp` 与 `.asm` 归档到 `outputs/<case>/`，并将可直接编码的结果写回同一目录。仍含符号寄存器占位符的文件会被显式跳过。

执行 `inline_asm_codegen` 后会先得到主分支约定的扁平输出：
- `output/ntt.cpp`
- `output/ntt.asm`
- `output/intt.cpp`
- `output/intt.asm`
- `...`

执行 `inline_asm_encode_outputs` 后，会进一步整理出根目录下的 `outputs/` 文件夹。执行 `hpu_reference_vectors` 后，各算子目录会补充输入、期望结果和可读 golden：
- `outputs/ntt/`
- `outputs/intt/`
- `outputs/mm/`
- `outputs/bconv/`
- `outputs/pmult/`
- `outputs/cmult/`
- `outputs/modup/`
- `outputs/moddown/`
- `outputs/auto/`
- `outputs/keyswitch/`
- `outputs/ciphertext_multiply/`
- `outputs/rv_interface_smoke/`

例如 `outputs/ntt/` 下会包含：
- `ntt.cpp`
- `ntt.asm`
- `ntt.inst32`（运行编码工具后生成）
- `test_data/input.bin`、`test_data/input.hex.txt`
- `test_data/expected.bin`、`test_data/expected.hex.txt`
- `test_data/params.json`、`test_data/artifact_manifest.csv`
- `test_data/hardware/hpu_mem_image.u32.bin`、`line_map.csv`、`hpu_mem_config.json`
- `test_data/hardware/mod_ctx_map.csv`、`twiddle_map.csv`、`abi.json`

---

## 3. 入口与参数

当前完整流程由三个阶段组成：

1. `inline_asm_codegen` 从 `src/main.cpp` 进入，向 `output/` 生成 `.cpp` 与 `.asm`。
2. `inline_asm_encode_outputs` 从 `test/encode/main.cpp` 进入，归档结果并把可编码 ASM 转成 `.inst32`。
3. `hpu_reference_vectors` 从 `test/reference/main.cpp` 进入，计算并验证 test data，然后写入 `outputs/<case>/test_data/`。

当前参数尚未收敛到单一配置文件：

- HPU 指令流参数位于 `src/main.cpp` 的 `kNttCfg`、`kPmultCfg`、`kCmultCfg`、`kModdownCfg`、`kAutoCfg` 和 `kCiphertextMultiplyCfg`。
- Reference 参数位于 `test/reference/main.cpp` 的 `kN`、`kNumQ`、`kNumP`、`kDnum`、`kPlainModulus` 和 `kSeed`。
- `outputs/*/test_data/params.json` 是生成结果，不是输入配置；直接修改后会在下一次生成时被覆盖。

修改 `N/Q/P/dnum` 时必须同步修改两处源配置，并满足 `N` 为 2 的幂、`num_q % dnum == 0`、`num_q + num_p <= 8`、所有 Q/P 模数可用 `uint32` 表示等约束。当前统一示例为 `N=4096, Q=4, P=3, dnum=2`。

`hpu_delivery` 会为 `ciphertext_multiply` 自动生成与主配置一致的 `N=4096, Q=4, P=3, dnum=2` 输入、评估密钥、阶段 golden、最终输出、明文校验和 artifact checksum。它同时生成独立的 `uint32` HPU_MEM 镜像、q/Barrett 上下文、逐 stage twiddle、256B line offset/count，并从同一 reference 拆分出 NTT、INTT、MM、BConv、ModUp、PMULT、CMULT、ModDown 和 KeySwitch 的独立 UT 数据包。`auto/test_data/STATUS.md` 记录该算子当前的寄存器分配阻塞项。


## 4. 关键设计实现说明

- **基于 HPU 对象槽的内存映射与原地 NTT/INTT：**
  底层不再关注向量的大块切片 `l`。针对 `stage=0~log2(N)-1` 的蝶形运算，`pntt/pintt` 以**第一个对象槽位作为数据对象**进行原地变换，**第二个对象槽位作为 twiddle 对象**。调用方只需确保每个 stage 前装载对应 twiddle。
  
- **切片感知的模提升运算：**
  为了支持分解字（Digit Decomposition），`modup` / `bconv` 在接口中新加入了 `q_offset` 参数与处理宽度 `num_q_digit`。使得在 `dnum > 1` 的外层循环下，基扩展算字能智能地识别应该处理当前分解下哪一部分素数环境与基偏移。
  
- **流水线的统一复用：**
  复杂的算子（如 `keyswitch`）不需要从头生成具体的 `hpu::pmul` 等语句。全部由底层统一拆解后的 `generate_hpu_*_body_asm` (Body Generator) 函数段拼接而成，避免了多次复制 DMA 及状态上下文切分代码。

- **完整密文乘法语义：**
  `cmult` 只负责 FHE 密文乘法中的张量积阶段，即 $t_0=a_0b_0$、$t_1=a_0b_1+a_1b_0$、$t_2=a_1b_1$。完整密文乘法由 `ciphertext_multiply` 负责：先将输入分量转换到 NTT 域做张量积，再回到系数域，对 $t_2$ 执行重线性化并合成标准二元密文 $(t_0+ks_0,\ t_1+ks_1)$。

- **生成与编码分层解耦：**
  `inline-asm` 仍负责汇编生成，`encode` 模块则负责解析、归一化和 32 位编码。两者保留独立边界，但通过同一 CMake 工程统一构建，从而降低汇编语义更新后生成器与编码器失配的风险。

- **双输入形式兼容：**
  编码器既可处理纯 ASM body，也可处理带有 `__asm__ volatile(...)` 包装的 C++ 内联汇编文本。对于 `void hpu_xxx(void) {`、`: "memory"`、`);` 等生成边界，解析器会做定向忽略；但非法汇编指令本身仍会被保留为错误。


---

## 5. 对象槽位与参数注意事项

调用方需要保证：

- `N` 为 2 的幂（NTT需要传入以确定 Stage 层数）
- 仅允许 3 个工作槽位：`p0/p1/p2`
- 复杂算子（PMULT/CMULT/MODUP/MODDOWN）使用 `dload/dstore` 流式搬运，不在本地长期保留多基对象
- `pmodld`/`pshcfg` 对应对象槽位已通过 `dload` 准备好模上下文与 shuffle 配置
- 需要阶段收敛时使用 `psync`
- 当前 `.inst32` 输出仅覆盖可直接完成寄存器解析的 ASM；`auto` 仍含 `x_c0`、`x_offset`、`x_out` 等符号寄存器占位符，需在完成物理寄存器分配后再编码
- `cmult` 与 `ciphertext_multiply` 均已进入统一 `.asm -> .inst32` 生成链路；其中 `ciphertext_multiply` 要求 `num_q % dnum == 0` 且 `num_q + num_p <= 8`
- `ciphertext_multiply/test_data` 已由软件 reference 自动生成；二进制格式、shape 和校验值见其中的 `params.json` 与 `artifact_manifest.csv`
- 顶层 `.bin` 是 `uint64` 数学 golden；真正面向 HPU 加载的是 `test_data/hardware/` 下按 256B line 补齐的 `.u32.bin`
- `hardware/line_map.csv` 给出每个对象的 byte address、line offset 和 line count；`hpu_mem_config.json` 给出 HPU_MEM window 值和语义 CSR 编程顺序
- 当前完整乘法和大部分算子的 DMA 地址仍使用 `x0/x0` 占位；runtime 把 `line_map.csv` 的 line offset/count 绑定到 `rs1/rs2` 前，`.inst32` 是计算顺序流而不是可直接执行程序

---

## 6. 当前交付边界

软件侧已完成指令生成、编码、完整密文乘法/重线性化 reference golden、独立 `uint32` 硬件镜像、q/Barrett `mod_ctx`、逐 stage twiddle、256B line 映射、HPU_MEM window 配置和 RV 接口冒烟流。硬件直接执行仍依赖 CSR 数字偏移、指令 `rs1/rs2` 绑定、scratch 布局和 DMA/`psync` 完成关系；详细签字项见 `doc/HPU_TEST_DELIVERY.md`。
