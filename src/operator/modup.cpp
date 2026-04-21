#include "operator/modup.hpp"

#include "util/bconv.hpp"

#include <sstream>
#include <string>

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
    bool append_psync)
{
    // MODUP 等价于 BConv: Q -> P
    return generate_hpu_bconv_body_asm(
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
}

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
    bool append_psync)
{
    std::ostringstream asm_code;
    asm_code << "void hpu_modup_Q" << num_q << "_P" << num_p << "(void) {\n";

    if (num_q <= 0 || num_p <= 0) {
        asm_code << "    // Invalid config: require num_q/num_p > 0\n";
        asm_code << "}\n";
        return asm_code.str();
    }

    asm_code << "    __asm__ volatile(\n";
    asm_code << "        /* MODUP: BConv Q -> P */\n";
    asm_code << generate_hpu_modup_body_asm(
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
