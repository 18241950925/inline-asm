#include "ntt.hpp"
#include "hpu_asm.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

namespace {

bool is_power_of_two(int x)
{
    return x > 0 && (x & (x - 1)) == 0;
}

} // namespace

std::string generate_hpu_ntt_asm(
    int N,
    int obj_poly_a,
    int obj_poly_b,
    int mod_ctx_obj,
    int shf_cfg_obj,
    bool append_psync)
{
    std::ostringstream asm_code;

    asm_code << "void hpu_ntt_N" << N << "(void) {\n";

    if (!is_power_of_two(N)) {
        asm_code << "    // Invalid config: require power-of-two N\n";
        asm_code << "}\n";
        return asm_code.str();
    }

    const int logN = static_cast<int>(std::log2(static_cast<double>(N)));

    asm_code << "    __asm__ volatile(\n";

    // 新版 ISA：通过对象槽位装载模上下文与 Shuffle 配置
    asm_code << hpu::pmodld(mod_ctx_obj);
    asm_code << hpu::pshcfg(shf_cfg_obj);

    int src_obj = obj_poly_a;
    int dst_obj = obj_poly_b;

    // 新版 NTT：软件按 stage 显式推进，stage 内 twiddle/重排由硬件处理
    for (int stage = 0; stage < logN; ++stage) {
        asm_code << "\n        // ==========================================\n";
        asm_code << "        // Stage " << stage << " (Stage-level pntt)\n";
        asm_code << "        // ==========================================\n";
        asm_code << hpu::pntt(dst_obj, src_obj, stage, 0);
        std::swap(src_obj, dst_obj);
    }

    if (append_psync) {
        asm_code << hpu::psync(0);
    }

    asm_code << "\n        // Final result object slot: " << hpu::pobj(src_obj) << "\n";
    asm_code << "\n        // 结束\n";
    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";
    
    return asm_code.str();
}