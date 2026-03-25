#pragma once

#include <stdint.h>
#include <string>

std::string generate_hpu_ntt_asm(int N, int l, uint16_t base_addr, int mod_id);

// ---------------------------------------------------------
// [1] 初始化宏：切换模数上下文与 Twiddle 起点
// ---------------------------------------------------------
#define HPU_NTT_INIT(MOD_ID, TW_START_ID) \
    __asm__ volatile( \
        "pmodsw %[mod] \n\t"    /* 切换到当前多项式的模数上下文 */ \
        "ptwld %[tw] \n\t"      /* 装载本 Stage 的 Twiddle 因子起点 */ \
        : \
        : [mod] "i" (MOD_ID), \
          [tw] "i" (TW_START_ID) \
        : "memory" \
    )

// ---------------------------------------------------------
// [2] 跨向量 Stage (Stride >= l) 核心宏
// ---------------------------------------------------------
// 适用于蝶形运算的两个输入分布在不同 SRAM 块的情况
#define HPU_NTT_CROSS_BLOCK(ADDR_X, ADDR_Y) \
    __asm__ volatile( \
        "sload %[x], 1 \n\t"    /* 加载 X 支路到向量寄存器 p1 */ \
        "sload %[y], 2 \n\t"    /* 加载 Y 支路到向量寄存器 p2 */ \
        "pntt 1, 2, 0 \n\t"     /* 执行并行蝶形运算，结果就地覆盖 p1, p2 */ \
        "ptwid \n\t"            /* 推进内部 Twiddle 状态到下一个值 */ \
        "sstore 1, %[x] \n\t"   /* 写回 X 支路 */ \
        "sstore 2, %[y] \n\t"   /* 写回 Y 支路 (IE=0, 不触发中断) */ \
        : \
        : [x] "i" (ADDR_X), \
          [y] "i" (ADDR_Y) \
        : "memory" \
    )

// ---------------------------------------------------------
// [3] 向量内 Stage (Stride < l) 核心宏
// ---------------------------------------------------------
// 适用于需要 pshuf2 进行寄存器内数据重排的情况
#define HPU_NTT_SHUFFLE_BLOCK(SHF_CFG_ID, ADDR_X, ADDR_Y) \
    __asm__ volatile( \
        "sload %[x], 1 \n\t"    \
        "sload %[y], 2 \n\t"    \
        "pshcfg %[shf] \n\t"    /* 配置真实的 Shuffle 重排模板 */ \
        "pshuf2 1, 2, 0 \n\t"   /* 执行联合混洗，对齐蝶形运算数据位 */ \
        "pntt 1, 2, 0 \n\t"     /* 执行蝶形运算 */ \
        "ptwid \n\t"            /* 推进 Twiddle 状态 */ \
        /* 注意：如果硬件在写回前需要逆向洗牌恢复顺序，需在此处增加逆向 pshuf2 */ \
        "sstore 1, %[x] \n\t"   \
        "sstore 2, %[y] \n\t"   \
        : \
        : [shf] "i" (SHF_CFG_ID), \
          [x] "i" (ADDR_X), \
          [y] "i" (ADDR_Y) \
        : "memory" \
    )
