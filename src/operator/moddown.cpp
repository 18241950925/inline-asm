#include "operator/moddown.hpp"

#include "util/bconv.hpp"
#include "util/hpu_asm.hpp"

#include <sstream>
#include <string>

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
    bool append_psync)
{
    std::ostringstream asm_code;
    asm_code << "        /* MODDOWN stage-1: BConv P -> Q (correction term) */\n";

    // 复用 bconv 主体：输入基改为 P，目标基改为 Q。
    asm_code << generate_hpu_bconv_body_asm(
        num_p,
        num_q,
        obj_p_base,
        obj_tmp_base,
        obj_qcorr_base,
        obj_phat_inv_base,
        obj_phat_modq_base,
        mod_ctx_p_base,
        mod_ctx_q_base,
        false);

    asm_code << "        /* MODDOWN stage-2: q <- q - correction (mod q_i) */\n";
    for (int i = 0; i < num_q; ++i) {
        const int ctx_q = mod_ctx_q_base + i;
        const int q = obj_q_base + i;
        const int corr = obj_qcorr_base + i;

        asm_code << "        /* q_" << i << " */\n";
        asm_code << hpu::pmodld(ctx_q);
        asm_code << hpu::psub(q, q, corr);
    }

    if (append_psync) {
        asm_code << hpu::psync(0);
    }

    return asm_code.str();
}

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
    bool append_psync)
{
    std::ostringstream asm_code;
    asm_code << "void hpu_moddown_Q" << num_q << "_P" << num_p << "(void) {\n";

    if (num_q <= 0 || num_p <= 0) {
        asm_code << "    // Invalid config: require num_q/num_p > 0\n";
        asm_code << "}\n";
        return asm_code.str();
    }

    asm_code << "    __asm__ volatile(\n";
    asm_code << generate_hpu_moddown_body_asm(
        num_q,
        num_p,
        obj_q_base,
        obj_p_base,
        obj_tmp_base,
        obj_qcorr_base,
        obj_phat_inv_base,
        obj_phat_modq_base,
        mod_ctx_p_base,
        mod_ctx_q_base,
        append_psync);

    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";

    return asm_code.str();
}
