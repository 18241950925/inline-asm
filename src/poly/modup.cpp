#include "poly/modup.hpp"

#include "util/bconv.hpp"
#include "util/hpu_asm.hpp"

#include <sstream>
#include <string>
#include <vector>

std::string generate_hpu_modup_body_asm(
    int num_q_digit,
    int num_p,
    int q_offset,
    bool append_psync)
{
    // MODUP 等价于 BConv: Q -> P
    return generate_hpu_bconv_body_asm(
        num_q_digit,
        num_p,
        q_offset,
        append_psync);
}

std::string generate_hpu_hybrid_modup_body_asm(
    int num_q,
    int num_p,
    int num_q_digit,
    int q_offset,
    bool append_psync)
{
    std::ostringstream asm_code;
    if (num_q <= 0 || num_p <= 0 || num_q_digit <= 0 || q_offset < 0
        || q_offset + num_q_digit > num_q || num_q + num_p > 8) {
        asm_code << "        // Invalid hybrid ModUp config\n";
        return asm_code.str();
    }

    std::vector<int> source_contexts;
    std::vector<int> target_contexts;
    for (int i = 0; i < num_q_digit; ++i) {
        source_contexts.push_back(q_offset + i);
    }
    for (int i = 0; i < num_q; ++i) {
        if (i < q_offset || i >= q_offset + num_q_digit) {
            target_contexts.push_back(i);
        }
    }
    for (int i = 0; i < num_p; ++i) {
        target_contexts.push_back(num_q + i);
    }

    asm_code << "        /* HYBRID MODUP: Q_digit -> full Q union P */\n";
    asm_code << "        /* Retain source digit limbs in full-basis workspace */\n";
    for (int i = 0; i < num_q_digit; ++i) {
        asm_code << "        /* Copy Q context " << (q_offset + i) << " */\n";
        asm_code << hpu::dload("x0", "x0", 0, hpu::DataType::poly);
        asm_code << hpu::dstore("x0", "x0", 0, 1);
    }
    asm_code << generate_hpu_bconv_contexts_body_asm(
        source_contexts,
        target_contexts,
        false);

    if (append_psync) {
        asm_code << hpu::psync(0);
    }
    return asm_code.str();
}

std::string generate_hpu_modup_asm(
    int num_q_digit,
    int num_p,
    int q_offset,
    bool append_psync)
{
    std::ostringstream asm_code;
    asm_code << "void hpu_modup_Q" << num_q_digit << "_P" << num_p << "(void) {\n";

    if (num_q_digit <= 0 || num_p <= 0 || q_offset < 0
        || q_offset + num_q_digit + num_p > 8) {
        asm_code << "    // Invalid config: require positive bases within 3-bit context space\n";
        asm_code << "}\n";
        return asm_code.str();
    }

    asm_code << "    __asm__ volatile(\n";
    asm_code << "        /* MODUP: BConv Q -> P */\n";
    asm_code << generate_hpu_modup_body_asm(
        num_q_digit,
        num_p,
        q_offset,
        append_psync);

    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";

    return asm_code.str();
}
