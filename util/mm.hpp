#pragma once

#include <stdint.h>
#include <string>

std::string generate_hpu_mm_asm(
  int N,
  int l,
  uint16_t base_addr_a,
  uint16_t base_addr_b,
  uint16_t base_addr_c,
  int mod_id);

// 宏定义：处理一对长度为 l 的向量块
// ADDR_A, ADDR_B, ADDR_C 为硬编码的 SRAM 地址偏移 (15-bit)
#define HPU_MM_BLOCK(MOD_ID, ADDR_A, ADDR_B, ADDR_C) \
    asm volatile( \
        "pmodsw %[mod] \n\t"         /* 切换模上下文 */ \
        "sload %[a], 1 \n\t"         /* 加载 A 到 p1 */ \
        "sload %[b], 2 \n\t"         /* 加载 B 到 p2 */ \
        "pmul 1, 2, 3 \n\t"          /* p3 = p1 * p2 (mod q) */ \
        "sstore 3, %[c] \n\t"        /* 结果存回 SRAM_C */ \
        : \
        : [mod] "i" (MOD_ID), \
          [a] "i" (ADDR_A), \
          [b] "i" (ADDR_B), \
          [c] "i" (ADDR_C) \
        : "memory" \
    )
