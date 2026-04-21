#include "operator/cmult.hpp"

#include "util/hpu_asm.hpp"
#include "util/mm.hpp"
#include "util/ntt.hpp"

#include <cmath>
#include <sstream>
#include <string>

namespace {

bool is_power_of_two(int x)
{
    return x > 0 && (x & (x - 1)) == 0;
}

int final_slot_after_stages(int N, int obj_a, int obj_b)
{
    const int logN = static_cast<int>(std::log2(static_cast<double>(N)));
    return (logN % 2 == 0) ? obj_a : obj_b;
}

} // namespace

std::string generate_hpu_cmult_asm(
    int num_q,
    int obj_a0_base,
    int obj_a1_base,
    int obj_b0_base,
    int obj_b1_base,
    int obj_out0_base,
    int obj_out1_base,
    int obj_out2_base,
    int mod_ctx_q_base,
    bool append_psync)
{
    std::ostringstream asm_code;
    asm_code << "void hpu_cmult_Q" << num_q << "(void) {\n";

    if (num_q <= 0) {
        asm_code << "    // Invalid config: require num_q > 0\n";
        asm_code << "}\n";
        return asm_code.str();
    }

    asm_code << "    __asm__ volatile(\n";
    asm_code << "        /* CMULT: (a0,a1)*(b0,b1)->(out0,out1,out2) over basis Q */\n";

    for (int i = 0; i < num_q; ++i) {
        const int ctx = mod_ctx_q_base + i;
        const int a0 = obj_a0_base + i;
        const int a1 = obj_a1_base + i;
        const int b0 = obj_b0_base + i;
        const int b1 = obj_b1_base + i;
        const int out0 = obj_out0_base + i;
        const int out1 = obj_out1_base + i;
        const int out2 = obj_out2_base + i;

        asm_code << "        /* q_" << i << " */\n";
        asm_code << hpu::pmodld(ctx);
        asm_code << generate_hpu_mm_body_asm(out0, a0, b0);
        asm_code << generate_hpu_mm_body_asm(out2, a1, b1);
        asm_code << generate_hpu_mm_body_asm(out1, a0, b1);
        asm_code << hpu::pmac(out1, a1, b0);
    }

    if (append_psync) {
        asm_code << hpu::psync(0);
    }

    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";

    return asm_code.str();
}

std::string generate_hpu_cmult_ntt_asm(
    int N,
    int num_q,
    int obj_a0_base,
    int obj_a1_base,
    int obj_b0_base,
    int obj_b1_base,
    int obj_a0_buf_base,
    int obj_a1_buf_base,
    int obj_b0_buf_base,
    int obj_b1_buf_base,
    int obj_out0_base,
    int obj_out1_base,
    int obj_out2_base,
    int mod_ctx_q_base,
    int shf_cfg_q_base,
    bool append_psync)
{
    std::ostringstream asm_code;
    asm_code << "void hpu_cmult_ntt_N" << N << "_Q" << num_q << "(void) {\n";

    if (num_q <= 0 || !is_power_of_two(N)) {
        asm_code << "    // Invalid config: require num_q > 0 and power-of-two N\n";
        asm_code << "}\n";
        return asm_code.str();
    }

    asm_code << "    __asm__ volatile(\n";
    asm_code << "        /* CMULT+NTT: NTT(a0,a1,b0,b1) then point-wise multiply-accumulate */\n";

    for (int i = 0; i < num_q; ++i) {
        const int ctx = mod_ctx_q_base + i;
        const int shf = shf_cfg_q_base + i;

        const int a0 = obj_a0_base + i;
        const int a1 = obj_a1_base + i;
        const int b0 = obj_b0_base + i;
        const int b1 = obj_b1_base + i;
        const int a0_buf = obj_a0_buf_base + i;
        const int a1_buf = obj_a1_buf_base + i;
        const int b0_buf = obj_b0_buf_base + i;
        const int b1_buf = obj_b1_buf_base + i;

        const int a0_ntt = final_slot_after_stages(N, a0, a0_buf);
        const int a1_ntt = final_slot_after_stages(N, a1, a1_buf);
        const int b0_ntt = final_slot_after_stages(N, b0, b0_buf);
        const int b1_ntt = final_slot_after_stages(N, b1, b1_buf);

        const int out0 = obj_out0_base + i;
        const int out1 = obj_out1_base + i;
        const int out2 = obj_out2_base + i;

        asm_code << "        /* q_" << i << " */\n";
        asm_code << generate_hpu_ntt_body_asm(N, a0, a0_buf, ctx, shf, false);
        asm_code << generate_hpu_ntt_body_asm(N, a1, a1_buf, ctx, shf, false);
        asm_code << generate_hpu_ntt_body_asm(N, b0, b0_buf, ctx, shf, false);
        asm_code << generate_hpu_ntt_body_asm(N, b1, b1_buf, ctx, shf, false);

        asm_code << hpu::pmodld(ctx);
        asm_code << generate_hpu_mm_body_asm(out0, a0_ntt, b0_ntt);
        asm_code << generate_hpu_mm_body_asm(out2, a1_ntt, b1_ntt);
        asm_code << generate_hpu_mm_body_asm(out1, a0_ntt, b1_ntt);
        asm_code << hpu::pmac(out1, a1_ntt, b0_ntt);
    }

    if (append_psync) {
        asm_code << hpu::psync(0);
    }

    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";

    return asm_code.str();
}
