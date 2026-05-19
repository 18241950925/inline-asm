# 2026-05-18 编码模块接入与输出目录更新说明

本文件用于记录 `inline-asm` 项目在 2026 年 5 月 18 日完成的编码链路接入工作，说明 `encode` 模块如何并入当前工程、生成结果如何转为 32 位二进制文本，以及新的 `outputs/` 目录组织方式。

---

## 一、本次更新目标

此前项目仅负责生成 HPU 的 C++ 内联汇编与 ASM 文本，编码工具独立维护，容易出现汇编生成逻辑与机器码转译逻辑不同步的问题。

本次更新完成以下目标：

1. 将 `encode` 作为库模块接入 `inline-asm` 工程。
2. 让主生成流程产出的 ASM 可继续转译为 `.inst32` 二进制文本。
3. 在不改动主分支原有 `output/` 生成方式的前提下，由后处理阶段将各算子的 `cpp / asm / inst32 / test_data` 统一收敛到根目录 `outputs/` 下。
4. 对暂不具备直接编码条件的输出进行显式跳过，而不是生成错误机器码。

---

## 二、编码模块接入方式

### 1. 目录结构

新增的编码模块位于：

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

该模块保留独立边界，通过静态库 `hpu_encode` 对外提供能力。头文件直接放在 `encode/include/` 下，不再额外增加 `hpu/` 子目录。

### 2. 构建接入

顶层 `CMakeLists.txt` 增加：

```cmake
add_subdirectory(encode)
add_subdirectory(test/encode)
```

编码模块继续跟随当前项目的 C++17 标准，不额外提升工程语言版本。

---

## 三、当前支持的转译能力

### 1. 指令语义对齐

编码模块已与 2026 年 5 月 15 日更新后的指令文档保持一致：

- `pntt / pintt` 按 `pdata, ptwiddle, stage, idx1, mode` 解释。
- 第一个对象槽位表示原地变换的数据对象。
- 第二个对象槽位表示 twiddle 对象。
- 调试字段显示同步采用 `pdata / ptwiddle` 命名。

### 2. 输入形式

编码器当前可处理两类输入：

- 纯 ASM body 文件；
- 带 `__asm__ volatile(...)` 包装的 C++ 内联汇编文件。

对于 C++ 形式输入，解析器会跳过生成器产生的边界语句，例如：

- `void hpu_xxx(void) {`
- `__asm__ volatile(`
- `:`
- `: "memory"`
- `);`
- `}`

仅忽略明确的包装边界，不会吞掉真实的非法汇编指令。

---

## 四、二进制生成流程

### 1. 工具位置

新增测试编码工具：

```text
test/encode/main.cpp
```

构建后生成可执行文件：

```bash
./build/test/encode/inline_asm_encode_outputs
```

### 2. 推荐使用流程

先由主程序生成汇编：

```bash
./build/inline_asm_codegen both
```

此时仍先得到主分支原有的扁平输出：

```text
output/
├── ntt.cpp
├── ntt.asm
├── intt.cpp
├── intt.asm
└── ...
```

再由编码辅助工具归档文件，并将可编码 ASM 转成 32 位二进制文本：

```bash
./build/test/encode/inline_asm_encode_outputs
```

---

## 五、统一输出目录

为保留主分支生成逻辑，`src/main.cpp` 仍只负责写入 `output/`。运行编码辅助工具后，项目根目录会进一步整理出：

```text
outputs/
```

每个算子各自独立成目录。例如：

```text
outputs/ntt/
├── ntt.cpp
├── ntt.asm
├── ntt.inst32
└── test_data/
```

当前会自动创建以下目录：

| 目录 | 说明 |
| --- | --- |
| `outputs/ntt/` | NTT 生成结果 |
| `outputs/intt/` | INTT 生成结果 |
| `outputs/mm/` | 模乘基础样例 |
| `outputs/bconv/` | Basis Conversion |
| `outputs/pmult/` | 明密文乘法 |
| `outputs/cmult/` | 密文乘法 |
| `outputs/modup/` | 模提升 |
| `outputs/moddown/` | 模回缩 |
| `outputs/auto/` | 自同构 |
| `outputs/keyswitch/` | 密钥切换 |

每个目录都会预留空的 `test_data/` 子目录，用于后续放置输入数据、期望结果或测试辅助文件。

---

## 六、当前编码状态

### 1. 已可直接生成 `.inst32` 的输出

当前编码工具会处理：

| 算子 | 当前状态 |
| --- | --- |
| `ntt` | 可编码 |
| `intt` | 可编码 |
| `mm` | 可编码 |
| `bconv` | 可编码 |
| `pmult` | 可编码 |
| `modup` | 可编码 |
| `moddown` | 可编码 |
| `keyswitch` | 可编码 |

### 2. 当前显式跳过的输出

| 算子 | 原因 |
| --- | --- |
| `auto` | ASM 中仍包含 `x_c0`、`x_offset`、`x_out` 等符号地址寄存器占位符，尚未完成真实寄存器分配 |

### 3. 当前尚未生成完整 ASM body 的输出

| 算子 | 原因 |
| --- | --- |
| `cmult` | 当前 `main.cpp` 中仅生成 `.cpp` 包装文件，body ASM 输出仍处于注释状态 |

---

## 七、当前边界与后续建议

1. `auto` 在完成符号寄存器到物理寄存器的映射前，不应强行生成 `.inst32`。
2. `cmult` 若需要参与统一编码链路，应先恢复 body ASM 的生成。
3. `outputs/<case>/test_data/` 目前仅作为目录占位；测试数据格式和生成规则仍需后续单独确定。
4. 若后续需要一键生成全部交付物，可再增加统一构建目标，将汇编生成与二进制转译串联起来。
