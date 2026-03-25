#include "bconv.hpp"

#include <sstream>
#include <string>

std::string generate_hpu_bconv_asm(
    int N,
    int l,
    uint16_t base_addr_in,
    uint16_t base_addr_acc,
    int mod_in,
    int mod_out,
    int cst_base_id)
{
    std::ostringstream asm_code;
    int num_vecs = N / l;

    asm_code << "void hpu_bconv_complete_N" << N << "_l" << l << "(void) {\n";
    asm_code << "    __asm__ volatile(\n";

    for (int i = 0; i < num_vecs; ++i) {
        uint16_t addr_in = static_cast<uint16_t>(base_addr_in + i);
        uint16_t addr_acc = static_cast<uint16_t>(base_addr_acc + i);
        int cst_id = cst_base_id + i;

        asm_code << "        /* Vec " << i << " */\n";
        asm_code << "        \"pmodsw " << mod_in << " \\n\\t\"\n";
        asm_code << "        \"sload " << addr_in << ", 1 \\n\\t\"\n";
        asm_code << "        \"pbcast " << cst_id << ", 2 \\n\\t\"\n";
        asm_code << "        \"pmul 1, 2, 3 \\n\\t\"\n";
        asm_code << "        \"pmodsw " << mod_out << " \\n\\t\"\n";
        asm_code << "        \"sload " << addr_acc << ", 4 \\n\\t\"\n";
        asm_code << "        \"padd 4, 3, 4 \\n\\t\"\n";
        asm_code << "        \"sstore 4, " << addr_acc << " \\n\\t\"\n";
    }

    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";

    return asm_code.str();
}
