#include "operator/modup.hpp"

#include "util/bconv.hpp"

#include <sstream>
#include <string>

std::string generate_hpu_modup_body_asm(
    int num_q,
    int num_p,
    bool append_psync)
{
    // MODUP 等价于 BConv: Q -> P
    return generate_hpu_bconv_body_asm(
        num_q,
        num_p,
        append_psync);
}

std::string generate_hpu_modup_asm(
    int num_q,
    int num_p,
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
        append_psync);

    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";

    return asm_code.str();
}
