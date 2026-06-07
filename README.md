# HPU Inline Assembly Codegen

本项目用于为全同态加密在自研 HPU（Homomorphic Processing Unit）硬件上，提供 C++ 内联汇编（Inline Assembly）的生成器。项目基于 HPU 的 3-bit 寻址槽位和流水线执行语义进行抽象，支持从底层通用计算直至高层复杂 FHE 算子的自动代码生成，并可将生成后的 ASM 继续转译为 32 位指令编码文本。

## 1. 项目结构

代码按照功能依赖分层，并分别维护在 `include` 及 `src` 目录下，包含三大类生成模块，以及独立的编码与测试辅助模块：

### 1) 基础工具层 (`util`)
- **`util/hpu_asm.hpp/cpp`**：基础 HPU 汇编助记符封装和生成接口，遵循《HPU_INSTRUCTION_MANUAL.md》。
- **`util/ntt.hpp/cpp`**：按 stage 推进的基于对象槽位语义的 NTT / INTT 汇编生成。
- **`util/mm.hpp/cpp`**：对象槽位级别的四则运算，特别是逐点向量乘法、乘加积累等（`pmul` / `pmac`）。
- **`util/bconv.hpp/cpp`**：对象槽位上带参数 `q_offset` 支持分组扩展的基础 Basis Conversion 两阶段汇编生成。

### 2) 多项式级操作层 (`poly`)
调用并复用 `util` 层的基础算子，组合出同态相关的复杂多项式算子：
- **`poly/pmult.hpp/cpp`**：明密文相乘 (Plaintext-Ciphertext Multiplication)。
- **`poly/cmult.hpp/cpp`**：密文相乘 (Ciphertext-Ciphertext Multiplication)。
- **`poly/modup.hpp/cpp`**：模提升 (ModUp) 操作，负责将 $Q$ 基下的残差提升扩展到 $Q \cup P$。
- **`poly/moddown.hpp/cpp`**：模回缩 (ModDown) 操作，负责将中间结果缩放回 $Q$ 基，纠正缩放因子。

### 3) 高级算子层 (`operator`)
拼装多项式级与基础工具算子，完整实现核心同态运算：
- **`operator/keyswitch.hpp/cpp`**：完整密钥切换 (KeySwitch) 逻辑生成，包含切片循环（密文分解，由参数 `dnum` 控制）、ModUp、多基 NTT、与 Evk (评估密钥) 点乘累加、INTT、以及 ModDown 缩放累加操作的全工作流流水线。

### 4) 指令编码模块 (`encode`)
将生成出的 HPU 汇编进一步转译为 32 位机器码文本：
- **`encode/include/*.hpp`**：定义指令数据结构、解析、编码及组装接口。
- **`encode/src/*.cpp`**：实现 ASM / C++ 内联汇编解析、格式归一化以及 `custom0` / `custom1` 指令编码。
- **`hpu_encode`**：由 `encode/CMakeLists.txt` 生成的静态库，供后续测试或上层流程复用。

### 5) 编码测试辅助模块 (`test/encode`)
用于把主生成流程输出的 ASM 继续转换为 `.inst32` 文件：
- **`test/encode/main.cpp`**：读取主流程生成的 `output/<case>.cpp` 与 `output/<case>.asm`，归档到 `outputs/<case>/`，再调用 `hpu_encode` 生成对应的 32 位二进制文本。
- **`inline_asm_encode_outputs`**：构建后生成的测试编码工具。

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

主生成程序仍保持主分支的输出方式，先在根目录生成扁平的 `output/` 文件夹；编码辅助工具随后会把 `.cpp` 与 `.asm` 归档到 `outputs/<case>/`，并将可直接编码的结果写回同一目录。仍含符号寄存器占位符的文件会被显式跳过。

执行 `inline_asm_codegen` 后会先得到主分支约定的扁平输出：
- `output/ntt.cpp`
- `output/ntt.asm`
- `output/intt.cpp`
- `output/intt.asm`
- `...`

执行 `inline_asm_encode_outputs` 后，会进一步整理出根目录下的 `outputs/` 文件夹。每个算子拥有独立子目录，目录中可同时存放对应的 C++ 内联汇编、ASM、二进制编码以及测试数据占位目录：
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

例如 `outputs/ntt/` 下会包含：
- `ntt.cpp`
- `ntt.asm`
- `ntt.inst32`（运行编码工具后生成）
- `test_data/`

---

## 3. 当前示例参数（main.cpp）

入口文件 `src/main.cpp` 对所有操作函数进行了组合测试。

当前示例生成流程由两阶段组成：

1. `inline_asm_codegen` 按主分支约定先在 `output/` 下生成 `.cpp` 与 `.asm`。
2. `inline_asm_encode_outputs` 负责将它们归档到 `outputs/<case>/`，并把可直接编码的 `.asm` 继续转译为 `.inst32`。

当前先为`outputs/ntt/test_data/`自动生成N=8的小规模NTT测试数据，其余算子目录仍保留`test_data/`占位。


## 4. 关键设计实现说明

- **基于 HPU 对象槽的内存映射与原地 NTT/INTT：**
  底层不再关注向量的大块切片 `l`。针对 `stage=0~log2(N)-1` 的蝶形运算，`pntt/pintt` 以**第一个对象槽位作为数据对象**进行原地变换，**第二个对象槽位作为 twiddle 对象**。调用方只需确保每个 stage 前装载对应 twiddle。
  
- **切片感知的模提升运算：**
  为了支持分解字（Digit Decomposition），`modup` / `bconv` 在接口中新加入了 `q_offset` 参数与处理宽度 `num_q_digit`。使得在 `dnum > 1` 的外层循环下，基扩展算字能智能地识别应该处理当前分解下哪一部分素数环境与基偏移。
  
- **流水线的统一复用：**
  复杂的算子（如 `keyswitch`）不需要从头生成具体的 `hpu::pmul` 等语句。全部由底层统一拆解后的 `generate_hpu_*_body_asm` (Body Generator) 函数段拼接而成，避免了多次复制 DMA 及状态上下文切分代码。

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
- 当前 `cmult` 仅生成 `.cpp` 包装文件，body ASM 输出仍处于注释状态，因此还未进入统一 `.inst32` 生成链路
- `outputs/ntt/test_data/`已包含N=8的小规模NTT输入、twiddle、模上下文、期望输出和独立指令；其余算子的测试数据仍需按后续联调约定补充

---

## TODO:
统一密文到coff domain
