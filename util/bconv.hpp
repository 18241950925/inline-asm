#pragma once

#include <stdint.h>
#include <string>

std::string generate_hpu_bconv_asm(
  int N,
  int l,
  uint16_t base_addr_in,
  uint16_t base_addr_acc,
  int mod_in,
  int mod_out,
  int cst_base_id);

#define HPU_BCONV_MAC_BLOCK(MOD_IN, MOD_OUT, CST_ID, ADDR_IN, ADDR_ACC) \
    asm volatile( \
        /* 1. 在输入模数下执行标量乘 */ \
        "pmodsw %[m_in] \n\t"        \
        "sload %[in], 1 \n\t"        /* 加载多项式块到 p1 */ \
        "pbcast %[cst], 2 \n\t"      /* 将常量槽 CST_ID 的标量广播到 p2 */ \
        "pmul 1, 2, 3 \n\t"          /* p3 = p1 * 常量 (mod_in) */ \
        \
        /* 2. 在输出模数下进行累加 */ \
        "pmodsw %[m_out] \n\t"       \
        "sload %[acc], 4 \n\t"       /* 加载当前累加值到 p4 */ \
        "padd 4, 3, 4 \n\t"          /* p4 = p4 + p3 (mod_out) */ \
        "sstore 4, %[acc] \n\t"      /* 写回累加器 */ \
        : \
        : [m_in] "i" (MOD_IN), \
          [m_out] "i" (MOD_OUT), \
          [cst] "i" (CST_ID), \
          [in] "i" (ADDR_IN), \
          [acc] "i" (ADDR_ACC) \
        : "memory" \
    )
