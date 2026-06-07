# N16测试生成器

`N16`表示`logN=16`，实际多项式长度为`N=65536`。

生成器直接调用项目原有`generate_hpu_mm_asm()`和`generate_hpu_ntt_asm()`，输出精简的MM、NTT内联汇编、指令编码和二进制测试数据。

```powershell
cmake --build build --config Debug
.\build\test\N16\Debug\inline_asm_test_n16.exe
```

结果位于`test/N16/generated/`。
