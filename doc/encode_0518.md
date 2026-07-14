# 2026-05-18 编码模块接入与当前交付流程

本文件记录 2026 年 5 月 18 日完成的编码链路接入工作，并按当前工程结构说明 HPU 指令生成、32 位编码、FHE 软件 reference 和测试数据交付流程。

## 1. 当前结论

工程当前有三个独立程序入口：

| 可执行文件 | 源入口 | 职责 |
| --- | --- | --- |
| `inline_asm_codegen` | `src/main.cpp` | 生成 HPU C++ 内联汇编与 ASM body |
| `inline_asm_encode_outputs` | `test/encode/main.cpp` | 归档输出、编码 `.inst32`、生成 RV 接口冒烟流 |
| `hpu_reference_vectors` | `test/reference/main.cpp` | 生成并自校验 FHE golden、UT/IT 输入输出数据 |

`test/reference/main.cpp` 没有替代 `src/main.cpp`。前者是软件算法 reference 的入口，后者仍是 HPU 指令流生成入口。顶层 `hpu_delivery` 目标负责依次调用三个程序，并执行交付门禁检查。

## 2. 编码模块接入

编码模块位于：

```text
encode/
├── CMakeLists.txt
├── include/
│   ├── assembler.hpp
│   ├── encoder.hpp
│   ├── instruction.hpp
│   └── parser.hpp
└── src/
    ├── assembler.cpp
    ├── encoder.cpp
    ├── instruction.cpp
    └── parser.cpp
```

该模块通过静态库 `hpu_encode` 提供解析和编码能力。顶层构建同时接入：

```cmake
add_subdirectory(encode)
add_subdirectory(test/encode)
add_subdirectory(test/reference)
```

编码器可处理纯 ASM body 和带 `__asm__ volatile(...)` 包装的 C++ 内联汇编。对 C++ 输入只忽略函数与内联汇编包装边界，真实的非法指令仍会报错。

## 3. 推荐执行方式

完整交付使用统一目标：

```bash
cmake -S . -B build
cmake --build build -j --target hpu_delivery
ctest --test-dir build --output-on-failure
```

`hpu_delivery` 的执行顺序为：

1. `inline_asm_codegen both`：向 `output/` 写入 `.cpp` 与 `.asm`。
2. `inline_asm_encode_outputs`：归档到 `outputs/<case>/`，并生成 `.inst32` 与 RV 冒烟流。
3. `hpu_reference_vectors`：生成完整密文乘法 reference，并拆分各算子 UT 数据包。
4. `check_delivery.cmake`：检查必要文件、算法验证结果和指令编码数量。

需要单步调试时，可手工执行：

```bash
./build/inline_asm_codegen both
./build/test/encode/inline_asm_encode_outputs
./build/test/reference/hpu_reference_vectors \
  outputs/ciphertext_multiply/test_data outputs
```

## 4. 输出目录

`src/main.cpp` 保留原有扁平输出：

```text
output/
├── ntt.cpp
├── ntt.asm
├── ciphertext_multiply.cpp
├── ciphertext_multiply.asm
└── ...
```

编码与 reference 阶段进一步整理出：

```text
outputs/<case>/
├── <case>.cpp
├── <case>.asm
├── <case>.inst32
└── test_data/
    ├── README.md
    ├── params.json
    ├── artifact_manifest.csv
    ├── *.bin
    └── *.hex.txt
```

具体文件名随算子变化。`.bin` 是硬件加载使用的小端 `uint64_t` 规范剩余数数组；每个二进制均有带用途、shape、维度和分块注释的 `.hex.txt` 人工可读版本。`artifact_manifest.csv` 记录路径、shape、字节数与校验值。

当前覆盖目录包括 `ntt`、`intt`、`mm`、`bconv`、`pmult`、`cmult`、`modup`、`moddown`、`keyswitch`、`auto`、`ciphertext_multiply` 和 `rv_interface_smoke`。

完整密文乘法目录额外包含：

- `memory_map.json`：待 RTL/DMA ABI 确认的建议逻辑地址映射。
- `dma_plan.csv`：各 FHE 阶段的输入、输出、域和基顺序。
- `VALIDATION.txt`：解密一致性与阶段验证结果。
- `input/`、`constants/`、`expected/`：密文、重线性化密钥和中间/最终 golden。

## 5. 当前编码状态

| 算子 | ASM | `.inst32` | reference test data |
| --- | --- | --- | --- |
| `ntt/intt/mm/bconv/pmult/cmult/modup/moddown/keyswitch` | 已生成 | 已生成 | 已生成 |
| `ciphertext_multiply` | 已生成 | 已生成 | 已生成完整 FHE 流程数据 |
| `auto` | 已生成 | 显式跳过 | `STATUS.md` 记录阻塞项 |
| `rv_interface_smoke` | 已生成 | 已生成 | decode 期望与非法输入用例 |

`auto` 的 ASM 仍含 `x_c0`、`x_offset`、`x_out` 等符号 DMA 寄存器，完成物理寄存器分配前不能可靠编码。

## 6. 参数配置

参数目前来自两处源配置：

- `src/main.cpp`：HPU 指令流生成参数。
- `test/reference/main.cpp`：FHE reference 和测试向量参数。

`outputs/*/test_data/params.json` 是生成结果，不是输入配置，重新生成时会覆盖。修改 `N/Q/P/dnum` 时需同步修改上述两处，并满足 `N` 为 2 的幂、`num_q % dnum == 0`、`num_q + num_p <= 8` 等当前实现约束。默认完整乘法参数为 `N=4096, Q=4, P=3, dnum=2`。

## 7. 当前交付边界

软件侧已经完成指令生成、`.inst32` 编码、完整密文乘法与重线性化 reference、算子 UT 数据和 RV 接口冒烟流。以下信息仍需硬件侧确认后才能把当前计算顺序流变成可直接执行程序：

1. `dload/dstore` 的 DDR 地址寄存器和偏移 ABI；当前完整乘法中仍使用 `x0/x0` 占位。
2. `mod_ctx`、NTT twiddle、shuffle 配置和评估密钥的物理打包格式。
3. HPU SRAM/scratch 容量、对象槽位驻留规则与覆盖时机。
4. DMA 完成、`psync`、指令发射和异常处理的精确关系。

完整验收项和硬件联调签字表见 `doc/HPU_TEST_DELIVERY.md`。
