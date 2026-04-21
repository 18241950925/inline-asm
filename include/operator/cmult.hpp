#pragma once

#include <string>

// CMULT: 密文密文乘法（2x2 -> 3）
// 对每个RNS基执行：
//   out0 = a0 * b0
//   out1 = a0 * b1 + a1 * b0
//   out2 = a1 * b1
std::string generate_hpu_cmult_asm(
    int num_q,
    int obj_a0_base,
    int obj_a1_base,
    int obj_b0_base,
    int obj_b1_base,
    int obj_out0_base,
    int obj_out1_base,
    int obj_out2_base,
    int mod_ctx_q_base,
    bool append_psync = false);

// CMULT + NTT前处理：输入在系数域，先对 (a0,a1,b0,b1) 做 NTT，再执行逐元素乘加。
// 输出 out0/out1/out2 位于 NTT 域。
std::string generate_hpu_cmult_ntt_asm(
    int N,
    int num_q,
    int obj_a0_base,
    int obj_a1_base,
    int obj_b0_base,
    int obj_b1_base,
    int obj_a0_buf_base,
    int obj_a1_buf_base,
    int obj_b0_buf_base,
    int obj_b1_buf_base,
    int obj_out0_base,
    int obj_out1_base,
    int obj_out2_base,
    int mod_ctx_q_base,
    int shf_cfg_q_base,
    bool append_psync = false);
