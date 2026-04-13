#pragma once

#include <stdint.h>
#include <string>

std::string generate_hpu_bconv_asm(
    int num_q,
    int num_p,
    int obj_q_base,
    int obj_tmp_base,
    int obj_p_base,
    int obj_qhat_inv_base,
    int obj_qhat_modp_base,
    int mod_ctx_q_base,
    int mod_ctx_p_base,
    bool append_psync = true);
