#pragma once

#include <string>

// MODUP: 基扩展 Q -> P
// 内部复用 bconv 两阶段流程。
std::string generate_hpu_modup_body_asm(
    int num_q,
    int num_p,
    int obj_q_base,
    int obj_tmp_base,
    int obj_p_base,
    int obj_qhat_inv_base,
    int obj_qhat_modp_base,
    int mod_ctx_q_base,
    int mod_ctx_p_base,
    bool append_psync = false);

std::string generate_hpu_modup_asm(
    int num_q,
    int num_p,
    int obj_q_base,
    int obj_tmp_base,
    int obj_p_base,
    int obj_qhat_inv_base,
    int obj_qhat_modp_base,
    int mod_ctx_q_base,
    int mod_ctx_p_base,
    bool append_psync = false);
