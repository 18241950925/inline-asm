#pragma once

#include <string>

// PMULT: 密文明文乘法
// 假设输入密文为两分量(ct0, ct1)，明文为pt，均按RNS基Q分布为 num_q 个对象槽位。
// 输出为(out0, out1)，对应每个基下逐元素模乘结果。
std::string generate_hpu_pmult_asm(
    int num_q,
    int obj_ct0_base,
    int obj_ct1_base,
    int obj_pt_base,
    int obj_out0_base,
    int obj_out1_base,
    int mod_ctx_q_base,
    bool append_psync = false);

// PMULT + NTT前处理：输入在系数域，先对 (ct0, ct1, pt) 做 NTT，再执行逐元素乘法。
// 输出 out0/out1 位于 NTT 域。
std::string generate_hpu_pmult_ntt_asm(
    int N,
    int num_q,
    int obj_ct0_base,
    int obj_ct1_base,
    int obj_pt_base,
    int obj_ct0_buf_base,
    int obj_ct1_buf_base,
    int obj_pt_buf_base,
    int obj_out0_base,
    int obj_out1_base,
    int mod_ctx_q_base,
    int shf_cfg_q_base,
    bool append_psync = false);
