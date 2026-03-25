#include "mm.hpp"

#include <sstream>
#include <string>

std::string generate_hpu_mm_asm(
    int N,
    int l,
    uint16_t base_addr_a,
    uint16_t base_addr_b,
    uint16_t base_addr_c,
    int mod_id)
{
    std::ostringstream asm_code;
    int num_vecs = N / l;

    asm_code << "void hpu_mm_complete_N" << N << "_l" << l << "(void) {\n";
    asm_code << "    __asm__ volatile(\n";
    asm_code << "        \"pmodsw " << mod_id << " \\n\\t\"\n";

    for (int i = 0; i < num_vecs; ++i) {
        uint16_t addr_a = static_cast<uint16_t>(base_addr_a + i*l);
        uint16_t addr_b = static_cast<uint16_t>(base_addr_b + i*l);
        uint16_t addr_c = static_cast<uint16_t>(base_addr_c + i*l);

        asm_code << "        /* Vec " << i << " */\n";
        asm_code << "        \"sload " << addr_a << ", p0 \\n\\t\"\n";
        asm_code << "        \"sload " << addr_b << ", p1 \\n\\t\"\n";
        asm_code << "        \"pmul p0, p1, p2 \\n\\t\"\n";
        asm_code << "        \"sstore p2, " << addr_c << " \\n\\t\"\n";
    }

    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";

    return asm_code.str();
}
