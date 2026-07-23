#include "util/ntt.hpp"
#include "util/hpu_asm.hpp"

#include <cmath>
#include <sstream>
#include <string>

namespace {

bool is_power_of_two(int x)
{
    return x > 0 && (x & (x - 1)) == 0;
}

} // namespace

std::string generate_hpu_ntt_body_asm(
    int N,
    int obj_poly,
    int twiddle_obj,
    bool append_psync)
{
    std::ostringstream asm_code;

    if (!is_power_of_two(N)) {
        asm_code << "        // Invalid config: require power-of-two N\n";
        return asm_code.str();
    }

    const int logN = static_cast<int>(std::log2(static_cast<double>(N)));

    int data_obj = obj_poly;

    asm_code << "        // dload all mod contexts (placeholder)\n";
    // 假设调用方已经加载过密文模数
    // obj_poly 是稳定的逻辑对象；控制器可为每个 stage 重分配物理 base。
    // 软件按 stage 显式推进，stage 内 twiddle/重排由硬件处理
    for (int stage = 0; stage < logN; ++stage) {
        asm_code << "\n        // ==========================================\n";
        asm_code << "        // Stage " << stage << " (Stage-level pntt)\n";
        asm_code << "        // ==========================================\n";
        asm_code << hpu::dload("x0", "x0", twiddle_obj, hpu::DataType::poly);
        asm_code << hpu::pntt(data_obj, twiddle_obj, stage, 0);
        asm_code << hpu::pfree(twiddle_obj);
    }

    if (append_psync) {
        asm_code << hpu::psync();
    }

    asm_code << "\n        // Final result object slot: " << hpu::pobj(data_obj) << "\n";
    return asm_code.str();
}

std::string generate_hpu_intt_body_asm(
    int N,
    int obj_poly,
    int twiddle_obj,
    bool append_psync)
{
    std::ostringstream asm_code;

    if (!is_power_of_two(N)) {
        asm_code << "        // Invalid config: require power-of-two N\n";
        return asm_code.str();
    }

    const int logN = static_cast<int>(std::log2(static_cast<double>(N)));

    int data_obj = obj_poly;

    asm_code << "        // dload all mod contexts (placeholder)\n";
    // 假设调用方已经加载过密文模数

    // obj_poly 是稳定的逻辑对象；控制器可为每个 stage 重分配物理 base。
    // 软件按 stage 显式推进，stage 内 twiddle/重排由硬件处理
    for (int stage = 0; stage < logN; ++stage) {
        asm_code << "\n        // ==========================================\n";
        asm_code << "        // Stage " << stage << " (Stage-level pintt)\n";
        asm_code << "        // ==========================================\n";
        asm_code << hpu::dload("x0", "x0", twiddle_obj, hpu::DataType::poly);
        asm_code << hpu::pintt(data_obj, twiddle_obj, stage, 0);
        asm_code << hpu::pfree(twiddle_obj);
    }

    if (append_psync) {
        asm_code << hpu::psync();
    }

    asm_code << "\n        // Final result object slot: " << hpu::pobj(data_obj) << "\n";
    return asm_code.str();
}

std::string generate_hpu_ntt_asm(
    int N,
    int obj_poly,
    int twiddle_obj,
    int mod_ctx_obj,
    bool append_psync)
{
    std::ostringstream asm_code;

    asm_code << "void hpu_ntt_N" << N << "(void) {\n";

    if (!is_power_of_two(N)) {
        asm_code << "    // Invalid config: require power-of-two N\n";
        asm_code << "}\n";
        return asm_code.str();
    }

    asm_code << "    __asm__ volatile(\n";
    asm_code << hpu::dload("x0", "x0", mod_ctx_obj, hpu::DataType::mod_ctx,
                           hpu::DloadFlag::small_bank);
    asm_code << hpu::psync();
    asm_code << hpu::pmodld(0);
    asm_code << generate_hpu_ntt_body_asm(
        N,
        obj_poly,
        twiddle_obj,
        false);
    asm_code << hpu::pfree(mod_ctx_obj);
    if (append_psync) {
        asm_code << hpu::psync();
    }
    asm_code << "\n        // 结束\n";
    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";
    
    return asm_code.str();
}

std::string generate_hpu_intt_asm(
    int N,
    int obj_poly,
    int twiddle_obj,
    int mod_ctx_obj,
    bool append_psync)
{
    std::ostringstream asm_code;

    asm_code << "void hpu_intt_N" << N << "(void) {\n";

    if (!is_power_of_two(N)) {
        asm_code << "    // Invalid config: require power-of-two N\n";
        asm_code << "}\n";
        return asm_code.str();
    }

    asm_code << "    __asm__ volatile(\n";
    asm_code << hpu::dload("x0", "x0", mod_ctx_obj, hpu::DataType::mod_ctx,
                           hpu::DloadFlag::small_bank);
    asm_code << hpu::psync();
    asm_code << hpu::pmodld(0);
    asm_code << generate_hpu_intt_body_asm(
        N,
        obj_poly,
        twiddle_obj,
        false);
    asm_code << hpu::pfree(mod_ctx_obj);
    if (append_psync) {
        asm_code << hpu::psync();
    }
    asm_code << "\n        // 结束\n";
    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";
    
    return asm_code.str();
}
