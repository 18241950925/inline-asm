## 1. 项目结构：

- `main.cpp`：给了 3 组示例参数，调用 3 个生成器并打印汇编文本
- `util/hpu_asm.hpp`：新版 HPU 汇编指令生成的公共接口文件，符合最新的 6 字段格式。
- `util/ntt.cpp`：按 stage 推进的 NTT 汇编生成（对象槽位语义）
- `util/mm.cpp`：对象槽位上的 `pmul` 汇编生成
- `util/bconv.cpp`：对象槽位上的 Basis Conversion 两阶段汇编生成

对应接口（已根据最新 HPU 架构移除切分参数 `l` 和部分不需要的外层控制参数）：

- `generate_hpu_ntt_asm(...)`
- `generate_hpu_mm_asm(...)`
- `generate_hpu_bconv_asm(...)`

并且新增了由生成器生成的详细手册：`HPU_INSTRUCTION_MANUAL.md` 供负责后端的同事将其转译成机器码。

---

## 2. 一键构建与运行

```bash
mkdir -p build
cd build
cmake ..
cmake --build . -j
./inline_asm_codegen
```

运行后会打印三段文本：

- `===== NTT ASM =====`
- `===== MM ASM =====`
- `===== BCONV ASM =====`

每段都是可以嵌入 C/C++ 的 `__asm__ volatile(...)` 字符串代码。

---

## 3. 当前示例参数（main.cpp）

### NTT
- `N = 64`
- `obj_poly_a = 0`
- `obj_poly_b = 1`
- `mod_ctx_obj = 2`
- `shf_cfg_obj = 3`

### MM
- `obj_a = 0`
- `obj_b = 1`
- `obj_c = 2`
- `mod_ctx_obj = 3`

### BCONV
- `num_q = 1`
- `num_p = 1`
- `obj_q_base = 0`
- `obj_tmp_base = 1`
- `obj_p_base = 2`
- `obj_qhat_inv_base = 3`
- `obj_qhat_modp_base = 4`
- `mod_ctx_q_base = 5`
- `mod_ctx_p_base = 6`

---

## 4. 具体实现

## 4.1 NTT（`util/ntt.cpp`）

- 对齐 HPU 文档，改为 **对象槽位 + stage 指令粒度**，并且去除了参数 `l` 统一执行。
- 初始化阶段：
  - `pmodld p<mod_ctx_obj>, 0, 0`
  - `pshcfg p<shf_cfg_obj>, 0, 0`
- 主循环执行 `stage = 0..log2(N)-1`：
  - `pntt pdst, psrc, stage, 0, 0`
  - 通过两个对象槽位做 ping-pong（`obj_poly_a`/`obj_poly_b`）
- 可选追加 `psync 0, 0` 作为阶段同步屏障

## 4.2 MM（`util/mm.cpp`）

- 彻底转为底层流式化对象处理，无需传入 `N` 和 `l` 按 `chunk` 模拟迭代。
- 入口先装载模上下文：`pmodld p<mod_ctx_obj>, 0, 0`
- 直接对于整个多项式对象生成 `pmul p<obj_a>, p<obj_b>, p<obj_c>`
- 可选追加 `psync 0, 0`

## 4.3 BCONV（`util/bconv.cpp`）

已移除内部基于 `chunk` 的计算过程，分两阶段执行：

- **Stage 1（Q 基预处理）**
  - 对每个 `q_j`：`pmodld p<mod_ctx_q_base + j>, 0, 0`
  - `x_j = a_j * q_hat_inv_j (mod q_j)`
  - 生成 `pmul p<obj_q_j>, p<obj_qhat_inv_j>, p<obj_tmp_j>`

- **Stage 2（P 基累加）**
  - 对每个 `p_i`：`pmodld p<mod_ctx_p_base + i>, 0, 0`
  - 累加 `x_j * q_hat_j(mod p_i)`
  - 第一次用 `pmul` 初始化累加，后续用 `pmac` 累加到 `p<obj_p_i>`
- 可选追加 `psync 0, 0`


---

## 5. 对象槽位与参数注意事项

本项目不会做越界检查，调用方需要保证：

- `N` 为 2 的幂（NTT需要传入以确定 Stage 层数）
- 各对象槽位 ID 与硬件对象管理保持一致
- `pmodld`/`pshcfg` 对应对象槽位已通过 `dload` 准备好模上下文与 shuffle 配置
- 需要阶段收敛时使用 `psync`

---
