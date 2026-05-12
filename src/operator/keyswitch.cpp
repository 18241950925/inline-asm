#include "operator/keyswitch.hpp"

#include "poly/modup.hpp"
#include "poly/moddown.hpp"
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

std::string generate_hpu_keyswitch_body_asm(
    int N,
    int num_q,
    int num_p,
    int dnum,
    bool append_psync)
{
    std::ostringstream asm_code;
    
    int total_bases = num_q + num_p;
    int digit_size = num_q / dnum;

    const int POBJ_MOD_CTX = 4;
    const int TWIDDLE = 3; // Dummy for shuffle cfg
    const int POBJ_TMP_A = 0;
    const int POBJ_TMP_B = 1;

    asm_code << hpu::dload("x0", "x0", POBJ_MOD_CTX, hpu::DataType::mod_ctx);

    asm_code << "        /* --- Decomposed digits loop (dnum = " << dnum << ") --- */\n";
    for (int d = 0; d < dnum; ++d) {
        int q_offset = d * digit_size;
        asm_code << "        /* --- Digit " << d << " --- */\n";

        // 1. ModUp (Q -> P)
        asm_code << "        /* --- Step 1: ModUp --- */\n";
        asm_code << generate_hpu_modup_body_asm(digit_size, num_p, q_offset, false);

        // 2. NTT
        asm_code << "        /* --- Step 2: NTT on Q and P bases --- */\n";

        for (int i = 0; i < total_bases; ++i) {
            asm_code << "        /* NTT ctx_" << i << " */\n";
            asm_code << hpu::pmodld(POBJ_MOD_CTX, i);
            asm_code << hpu::dload("x0", "x0", POBJ_TMP_A, hpu::DataType::poly);
            asm_code << hpu::dload("x0", "x0", TWIDDLE, hpu::DataType::poly);
            asm_code << generate_hpu_ntt_body_asm(N, POBJ_TMP_A, POBJ_TMP_B, POBJ_MOD_CTX, TWIDDLE, false);
            int final_slot = final_slot_after_stages(N, POBJ_TMP_A, POBJ_TMP_B);
            asm_code << hpu::dstore("x0", "x0", final_slot, 0);
        }

        // 3. Multiplication with Evk
        asm_code << "        /* --- Step 3: Multiply with Evaluation Key --- */\n";
        const int POBJ_CT = 0;
        const int POBJ_EVK = 1;
        const int POBJ_OUT = 2;

        for (int v = 0; v < 2; ++v) {
            asm_code << "        /* evk" << v << " mult for all bases */\n";
            for (int i = 0; i < total_bases; ++i) {
                asm_code << "        /* base_" << i << " */\n";
                asm_code << hpu::pmodld(POBJ_MOD_CTX, i);
                // IF first digit, just mul. If subsequent digits, multiply and accumulate (pmac)
                asm_code << hpu::dload("x0", "x0", POBJ_CT, hpu::DataType::poly);
                asm_code << hpu::dload("x0", "x0", POBJ_EVK, hpu::DataType::poly);
                if (d == 0) {
                    asm_code << generate_hpu_mm_body_asm(POBJ_OUT, POBJ_CT, POBJ_EVK);
                } else {
                    asm_code << hpu::dload("x0", "x0", POBJ_OUT, hpu::DataType::poly); // Load accumulated result
                    asm_code << hpu::pmac(POBJ_OUT, POBJ_CT, POBJ_EVK);
                }
                asm_code << hpu::dstore("x0", "x0", POBJ_OUT, 0);
            }
        }
    }

    // 4. INTT
    asm_code << "        /* --- Step 4: INTT on Q and P bases --- */\n";
    const int POBJ_MOD_CTX2 = 4;
    const int TWIDDLE2 = 3;
    const int POBJ_TMP_A2 = 0;
    const int POBJ_TMP_B2 = 1;
    asm_code << hpu::dload("x0", "x0", POBJ_MOD_CTX2, hpu::DataType::mod_ctx);
    for (int v = 0; v < 2; ++v) {
        asm_code << "        /* INTT for out" << v << " */\n";
        for (int i = 0; i < total_bases; ++i) {
            asm_code << "        /* INTT ctx_" << i << " */\n";
            asm_code << hpu::pmodld(POBJ_MOD_CTX2, i);
            asm_code << hpu::dload("x0", "x0", POBJ_TMP_A2, hpu::DataType::poly);
            asm_code << hpu::dload("x0", "x0", TWIDDLE2, hpu::DataType::poly);
            asm_code << generate_hpu_intt_body_asm(N, POBJ_TMP_A2, POBJ_TMP_B2, POBJ_MOD_CTX2, TWIDDLE2, false);
            int final_slot = final_slot_after_stages(N, POBJ_TMP_A2, POBJ_TMP_B2);
            asm_code << hpu::dstore("x0", "x0", final_slot, 0);
        }
    }

    // 5. ModDown
    asm_code << "        /* --- Step 5: ModDown for both parts --- */\n";
    for (int v = 0; v < 2; ++v) {
        asm_code << "        /* ModDown for out" << v << " */\n";
        asm_code << generate_hpu_moddown_body_asm(num_q, num_p, false);
    }
    asm_code << "        /* --- Step 6: Add c0 to out0 --- */\n";
    const int POBJ_MOD_CTX_S6 = 4;
    const int POBJ_OUT0 = 0;
    const int POBJ_C0 = 1;
    const int POBJ_FINAL_OUT0 = 2;

    asm_code << hpu::dload("x0", "x0", POBJ_MOD_CTX_S6, hpu::DataType::mod_ctx);
    
    for (int i = 0; i < num_q; ++i) { // 降模后只有 num_q 个基了
        asm_code << hpu::pmodld(POBJ_MOD_CTX_S6, i); // 切上下文
        // 1. 加载刚才 ModDown 生成的 out0
        asm_code << hpu::dload("x0", "x0", POBJ_OUT0, hpu::DataType::poly);
        // 2. 加载原始密文的 c0 分量
        asm_code << hpu::dload("x0", "x0", POBJ_C0, hpu::DataType::poly); 
        // 3. 在片上直接相加
        asm_code << hpu::padd(POBJ_FINAL_OUT0, POBJ_OUT0, POBJ_C0);
        // 4. 写回主存
        asm_code << hpu::dstore("x0", "x0", POBJ_FINAL_OUT0, 0);
    }

    if (append_psync) {
        asm_code << hpu::psync(0);
    }

    return asm_code.str();
}

std::string generate_hpu_keyswitch_asm(
    int N,
    int num_q,
    int num_p,
    int dnum,
    bool append_psync)
{
    std::ostringstream asm_code;
    asm_code << "void hpu_keyswitch_N" << N << "_Q" << num_q << "_P" << num_p << "_D" << dnum << "(void) {\n";

    if (num_q <= 0 || num_p <= 0 || !is_power_of_two(N) || dnum <= 0) {
        asm_code << "    // Invalid config: require num_q/num_p/dnum > 0 and power-of-two N\n";
        asm_code << "}\n";
        return asm_code.str();
    }

    asm_code << "    __asm__ volatile(\n";
    asm_code << "        /* KEYSWITCH: ModUp -> NTT -> Mult -> INTT -> ModDown */\n";

    asm_code << generate_hpu_keyswitch_body_asm(N, num_q, num_p, dnum, append_psync);

    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";

    return asm_code.str();
}
