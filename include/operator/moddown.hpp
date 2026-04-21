#pragma once

#include <string>

// MODDOWN: 基约减 (Q∪P) -> Q
// 方案：先将P基分量做 bconv(P->Q) 得到校正项，再在Q基执行 psub。
std::string generate_hpu_moddown_body_asm(
    int num_q,
    int num_p,
    int obj_q_base,
    int obj_p_base,
    int obj_tmp_base,
    int obj_qcorr_base,
    int obj_phat_inv_base,
    int obj_phat_modq_base,
    int mod_ctx_p_base,
    int mod_ctx_q_base,
    bool append_psync = false);

std::string generate_hpu_moddown_asm(
    int num_q,
    int num_p,
    int obj_q_base,
    int obj_p_base,
    int obj_tmp_base,
    int obj_qcorr_base,
    int obj_phat_inv_base,
    int obj_phat_modq_base,
    int mod_ctx_p_base,
    int mod_ctx_q_base,
    bool append_psync = false);
