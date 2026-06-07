# N16测试

`N16`表示`logN=16`,实际多项式长度为`N=65536`。系数和RNS模数均按32bit存储。

## 文件

- `mm.cpp`:原`generate_hpu_mm_asm()`生成的内联汇编。
- `mm.inst32`:MM的32bit指令编码。
- `mm_data.bin`:MM输入A、输入B和期望输出。
- `ntt.cpp`:原`generate_hpu_ntt_asm(N=65536)`生成的内联汇编。
- `ntt.inst32`:NTT的32bit指令编码,包含16个stage。
- `ntt_data.bin`:NTT输入、期望输出和16个stage的twiddle。

## 公共文件头

全部整数使用little-endian。

`magic[8],version:u32,logN:u32,N:u32,L:u32,coeff_bits:u32`

当前值:`logN=16,N=65536,L=2,coeff_bits=32`。

## MM数据布局

每个RNSlimb依次保存:`q:u32,psi:u32,omega:u32,A[N],B[N],expected[N]`。

## NTT数据布局

每个RNSlimb依次保存:`q:u32,psi:u32,omega:u32,input[N],expected[N],twiddle_count:u32`，随后保存16组`stage:u32,count:u32,twiddle[count]`。

模数为`4293918721`和`4291952641`，两者都支持`2×65536`阶单位根。MM执行前需由测试平台把当前limb的A、B、模上下文装入p0、p1、p4。q0和q1分别执行同一份指令。
