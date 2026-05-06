# HPU Inline Assembly Codegen

本项目用于为全同态加密在自研 HPU（Homomorphic Processing Unit）硬件上，提供 C++ 内联汇编（Inline Assembly）的生成器。项目基于 HPU 的 3-bit 寻址槽位和流水线执行语义进行抽象，支持从底层通用计算直至高层复杂 FHE 算子的自动代码生成。

## 1. 项目结构

代码按照功能依赖分层，并分别维护在 `include` 及 `src` 目录下，包含三大类模块：

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

---

## 2. 一键构建与运行

```bash
mkdir -p build
cd build
cmake ..
cmake --build . -j
./inline_asm_codegen
```

运行时程序会在运行级所在目录自动创建 `output/` 文件夹，并将所有的生成的汇编函数保存在其中（对应的 CPP 头文件包含了可直接被 GCC 编译的 `__asm__ volatile()` 块）：
- `output/ntt.cpp`
- `output/intt.cpp`
- `output/mm.cpp`
- `output/bconv.cpp`
- `output/pmult.cpp`
- `output/cmult.cpp`
- `output/modup.cpp`
- `output/moddown.cpp`
- `output/keyswitch.cpp`

---

## 3. 当前示例参数（main.cpp）

入口文件 `src/main.cpp` 对所有操作函数进行了组合测试。


## 4. 关键设计实现说明

- **基于 HPU 对象槽的内存映射与 Ping-Pong 特性：**
  底层不再关注向量的大块切片 `l`，针对 `stage=0~log2(N)-1` 级别的蝶形运算，使用两个槽位如 `TMP_A` 和 `TMP_B` 进行 Ping-Pong（滚动），并能在编译期判定 `stage` 结束后存放的准确槽位进行回写。
  
- **切片感知的模提升运算：**
  为了支持分解字（Digit Decomposition），`modup` / `bconv` 在接口中新加入了 `q_offset` 参数与处理宽度 `num_q_digit`。使得在 `dnum > 1` 的外层循环下，基扩展算字能智能地识别应该处理当前分解下哪一部分素数环境与基偏移。
  
- **流水线的统一复用：**
  复杂的算子（如 `keyswitch`）不需要从头生成具体的 `hpu::pmul` 等语句。全部由底层统一拆解后的 `generate_hpu_*_body_asm` (Body Generator) 函数段拼接而成，避免了多次复制 DMA 及状态上下文切分代码。


---

## 5. 对象槽位与参数注意事项

调用方需要保证：

- `N` 为 2 的幂（NTT需要传入以确定 Stage 层数）
- 仅允许 3 个工作槽位：`p0/p1/p2`
- 复杂算子（PMULT/CMULT/MODUP/MODDOWN）使用 `dload/dstore` 流式搬运，不在本地长期保留多基对象
- `pmodld`/`pshcfg` 对应对象槽位已通过 `dload` 准备好模上下文与 shuffle 配置
- 需要阶段收敛时使用 `psync`

---
