#include "operator/moddown.hpp"

#include "util/bconv.hpp"
#include "util/hpu_asm.hpp"

#include <sstream>
#include <string>

std::string generate_hpu_moddown_body_asm(
    int num_q,
    int num_p,
    bool append_psync)
{
    std::ostringstream asm_code;

    // 复用 HPU 硬件槽位
    const int POBJ_MOD_CTX = 4;
    const int POBJ_Q = 0;
    const int POBJ_CORR = 1;

    asm_code << "        /* MODDOWN stage-1: BConv P -> Q (correction term) */\n";
    // 复用 bconv 主体：输入基改为 P，目标基改为 Q。
    asm_code << generate_hpu_bconv_body_asm(
        num_p,
        num_q,
        false);

    asm_code << "        // dload all mod contexts (placeholder)\\n";
    asm_code << hpu::dload("x0", "x0", POBJ_MOD_CTX, hpu::DataType::mod_ctx);

    asm_code << "        /* MODDOWN stage-2: q <- q - correction (mod q_i) */\\n";
    for (int i = 0; i < num_q; ++i) {
        asm_code << "        /* q_" << i << " */\\n";
        asm_code << hpu::pmodld(POBJ_MOD_CTX, i);

        asm_code << hpu::dload("x0", "x0", POBJ_Q, hpu::DataType::poly);
        asm_code << hpu::dload("x0", "x0", POBJ_CORR, hpu::DataType::poly);
        
        asm_code << hpu::psub(POBJ_Q, POBJ_Q, POBJ_CORR);
        asm_code << hpu::dstore("x0", "x0", POBJ_Q, 0);
    }

    if (append_psync) {
        asm_code << hpu::psync(0);
    }

    return asm_code.str();
}

std::string generate_hpu_moddown_asm(
    int num_q,
    int num_p,
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
        append_psync);

    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";

    return asm_code.str();
}
