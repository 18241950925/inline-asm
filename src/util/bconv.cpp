#include "util/bconv.hpp"
#include "util/hpu_asm.hpp"
#include <sstream>
#include <string>

std::string generate_hpu_bconv_body_asm(
    int num_q,
    int num_p,
    int obj_q_base,
    int obj_tmp_base,
    int obj_p_base,
    int obj_qhat_inv_base,
    int obj_qhat_modp_base,
    int mod_ctx_q_base,
    int mod_ctx_p_base,
    bool append_psync)
{
    std::ostringstream asm_code;

    // ==========================================
    // 阶段一：预处理 (Pre-multiply)
    // 对每个输入基 q_j 计算: x_j = [a_j * q_hat_inv] mod q_j
    // ==========================================
    asm_code << "        /* --- STAGE 1: Precompute in Input Basis Q --- */\n";
    for (int j = 0; j < num_q; ++j) {
        const int obj_q = obj_q_base + j;
        const int obj_inv = obj_qhat_inv_base + j;
        const int obj_tmp = obj_tmp_base + j;
        const int obj_mod_ctx_q = mod_ctx_q_base + j;

        asm_code << "        /* Context: q_" << j << " */\n";
        asm_code << hpu::pmodld(obj_mod_ctx_q);
        asm_code << hpu::pmul(obj_tmp, obj_q, obj_inv);
    }

    // ==========================================
    // 阶段二：跨基累加 (Accumulate)
    // 对每个目标基 p_i 计算: Acc_i = SUM( x_j * q_hat_j ) mod p_i
    // ==========================================
    asm_code << "        /* --- STAGE 2: Accumulate in Target Basis P --- */\n";
    for (int i = 0; i < num_p; ++i) {
        const int obj_acc = obj_p_base + i;
        const int obj_mod_ctx_p = mod_ctx_p_base + i;

        asm_code << "        /* Context: p_" << i << " */\n";
        asm_code << hpu::pmodld(obj_mod_ctx_p);

        for (int j = 0; j < num_q; ++j) {
            const int obj_tmp = obj_tmp_base + j;
            const int obj_coeff = obj_qhat_modp_base + (i * num_q) + j;

            if (j == 0) {
                asm_code << hpu::pmul(obj_acc, obj_tmp, obj_coeff);
            } else {
                asm_code << hpu::pmac(obj_acc, obj_tmp, obj_coeff);
            }
        }
    }

    if (append_psync) {
        asm_code << hpu::psync(0);
    }

    return asm_code.str();
}

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
    bool append_psync)
{
    std::ostringstream asm_code;
    asm_code << "void hpu_full_bconv_Q" << num_q << "_P" << num_p << "(void) {\n";

    if (num_q <= 0 || num_p <= 0) {
        asm_code << "    // Invalid config: require num_q/num_p > 0\n";
        asm_code << "}\n";
        return asm_code.str();
    }

    asm_code << "    __asm__ volatile(\n";
    asm_code << generate_hpu_bconv_body_asm(
        num_q,
        num_p,
        obj_q_base,
        obj_tmp_base,
        obj_p_base,
        obj_qhat_inv_base,
        obj_qhat_modp_base,
        mod_ctx_q_base,
        mod_ctx_p_base,
        append_psync);

    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";

    return asm_code.str();
}