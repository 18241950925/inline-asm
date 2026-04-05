## 1. 项目结构：

- `main.cpp`：给了 3 组示例参数，调用 3 个生成器并打印汇编文本
- `util/ntt.cpp`：CG-NTT（Constant Geometry NTT）汇编生成
- `util/mm.cpp`：逐向量块乘法（`pmul`）汇编生成
- `util/bconv.cpp`：Basis Conversion 两阶段（预处理 + 跨基累加）汇编生成

对应接口：

- `generate_hpu_ntt_asm(...)`
- `generate_hpu_mm_asm(...)`
- `generate_hpu_bconv_asm(...)`

---

## 2. 一键构建与运行

```bash
cmake -S . -B build
cmake --build build -j
./build/inline_asm_codegen
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
- `l = 16`
- `base_addr_in = 0x0000`
- `base_addr_out = 0x0080`
- `mod_id = 0`

### MM
- `N = 64`
- `l = 16`
- `base_addr_a = 0x0100`
- `base_addr_b = 0x0200`
- `base_addr_c = 0x0300`
- `mod_id = 1`

### BCONV
- `N = 64`
- `l = 16`
- `num_q = 2`
- `num_p = 3`
- `base_addr_in = 0x0400`
- `base_addr_tmp = 0x0500`
- `base_addr_out = 0x0600`

---

## 4. 具体实现

## 4.1 NTT（`util/ntt.cpp`）

- 使用 CG-NTT 思路，循环 `stage = 1..log2(N)`
- 每个 stage：
  - `ptwld i`
  - `sload x -> p0`，`sload y -> p1`
  - `pntt p0, p1, p0`
  - `pshuf2 p0, p1, p0`
  - `sstore p0/p1`
- stage 结束发 `ptwid`
- 输入/输出缓冲区采用 **Ping-Pong** 翻转（`base_addr_in` 与 `base_addr_out` 交替读写）

> 备注：代码里 `pshcfg` 固定用了 `shf_id_perfect = 0x00`，如果硬件模板 ID 不同，请改这里。

## 4.2 MM（`util/mm.cpp`）

每个向量块固定序列：

1. `sload A -> p0`
2. `sload B -> p1`
3. `pmul p0, p1, p2`
4. `sstore p2 -> C`

入口只做一次 `pmodsw mod_id`。

## 4.3 BCONV（`util/bconv.cpp`）

分两阶段：

- **Stage 1（Q 基预处理）**
  - 对每个 `q_j`：`pmodsw slot_q`
  - `x_j = a_j * q_hat_inv (mod q_j)`
  - 结果写到 `base_addr_tmp`

- **Stage 2（P 基累加）**
  - 对每个 `p_i`：`pmodsw slot_p`
  - 累加 `x_j * q_hat_j(mod p_i)`
  - 第一次用 `pmul`，后续用 `pmac` 累加到 `p2`
  - 最终写到 `base_addr_out`

当前代码里的槽位/常量寄存器是原型约定：

- 模数槽位：通过 `j % 4`、`(num_q + i) % 4` 分配
- `q_hat_inv` 默认 `c0`
- `q_hat_j(mod p_i)` 默认 `c1`


---

## 5. 地址与容量注意事项

本项目不会做越界检查，调用方需要保证：

- `N % l == 0`
- 各缓冲区容量足够（按 `num_vecs = N / l` 计算）
- 各地址区间不重叠（尤其是 NTT 的双缓冲、BCONV 的 in/tmp/out）
- `mod_id`、`pshcfg`、常量寄存器 ID 在硬件侧已正确预置

---

