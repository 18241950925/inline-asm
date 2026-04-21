#include "operator/pmult.hpp"

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

std::string generate_hpu_pmult_asm(
    int num_q,
    int obj_ct0_base,
    int obj_ct1_base,
    int obj_pt_base,
    int obj_out0_base,
    int obj_out1_base,
    int mod_ctx_q_base,
    bool append_psync)
{
    std::ostringstream asm_code;
    asm_code << "void hpu_pmult_Q" << num_q << "(void) {\n";

    if (num_q <= 0) {
        asm_code << "    // Invalid config: require num_q > 0\n";
        asm_code << "}\n";
        return asm_code.str();
    }

    asm_code << "    __asm__ volatile(\n";
    asm_code << "        /* PMULT: (ct0, ct1) * pt over RNS basis Q */\n";

    for (int i = 0; i < num_q; ++i) {
        const int ctx = mod_ctx_q_base + i;
        const int ct0 = obj_ct0_base + i;
        const int ct1 = obj_ct1_base + i;
        const int pt = obj_pt_base + i;
        const int out0 = obj_out0_base + i;
        const int out1 = obj_out1_base + i;

        asm_code << "        /* q_" << i << " */\n";
        asm_code << hpu::pmodld(ctx);
        asm_code << generate_hpu_mm_body_asm(out0, ct0, pt);
        asm_code << generate_hpu_mm_body_asm(out1, ct1, pt);
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

std::string generate_hpu_pmult_ntt_asm(
    int N,
    int num_q,
    int obj_ct0_base,
    int obj_ct1_base,
    int obj_pt_base,
    int obj_ct0_buf_base,
    int obj_ct1_buf_base,
    int obj_pt_buf_base,
    int obj_out0_base,
    int obj_out1_base,
    int mod_ctx_q_base,
    int shf_cfg_q_base,
    bool append_psync)
{
    std::ostringstream asm_code;
    asm_code << "void hpu_pmult_ntt_N" << N << "_Q" << num_q << "(void) {\n";

    if (num_q <= 0 || !is_power_of_two(N)) {
        asm_code << "    // Invalid config: require num_q > 0 and power-of-two N\n";
        asm_code << "}\n";
        return asm_code.str();
    }

    asm_code << "    __asm__ volatile(\n";
    asm_code << "        /* PMULT+NTT: NTT(ct0,ct1,pt) then point-wise multiply */\n";

    for (int i = 0; i < num_q; ++i) {
        const int ctx = mod_ctx_q_base + i;
        const int shf = shf_cfg_q_base + i;

        const int ct0 = obj_ct0_base + i;
        const int ct1 = obj_ct1_base + i;
        const int pt = obj_pt_base + i;
        const int ct0_buf = obj_ct0_buf_base + i;
        const int ct1_buf = obj_ct1_buf_base + i;
        const int pt_buf = obj_pt_buf_base + i;

        const int ct0_ntt = final_slot_after_stages(N, ct0, ct0_buf);
        const int ct1_ntt = final_slot_after_stages(N, ct1, ct1_buf);
        const int pt_ntt = final_slot_after_stages(N, pt, pt_buf);

        const int out0 = obj_out0_base + i;
        const int out1 = obj_out1_base + i;

        asm_code << "        /* q_" << i << " */\n";
        asm_code << generate_hpu_ntt_body_asm(N, ct0, ct0_buf, ctx, shf, false);
        asm_code << generate_hpu_ntt_body_asm(N, ct1, ct1_buf, ctx, shf, false);
        asm_code << generate_hpu_ntt_body_asm(N, pt, pt_buf, ctx, shf, false);
        asm_code << hpu::pmodld(ctx);
        asm_code << generate_hpu_mm_body_asm(out0, ct0_ntt, pt_ntt);
        asm_code << generate_hpu_mm_body_asm(out1, ct1_ntt, pt_ntt);
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
