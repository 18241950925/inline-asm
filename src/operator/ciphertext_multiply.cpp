#include "operator/ciphertext_multiply.hpp"

#include "poly/cmult.hpp"
#include "poly/moddown.hpp"
#include "poly/modup.hpp"
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

bool is_valid_config(int N, int num_q, int num_p, int dnum)
{
    return is_power_of_two(N) && num_q > 0 && num_p > 0 && dnum > 0
        && num_q % dnum == 0 && num_q + num_p <= hpu::kMaxModContexts;
}

std::string generate_basis_ntt_body_asm(
    int N,
    int num_q,
    int num_components,
    const std::string& label)
{
    std::ostringstream asm_code;

    const int POBJ_POLY = 0;
    const int POBJ_TWIDDLE = 3;
    const int POBJ_MOD_CTX = 4;

    asm_code << "        /* --- " << label << ": coefficient -> NTT domain --- */\n";
    asm_code << hpu::dload("x0", "x0", POBJ_MOD_CTX, hpu::DataType::mod_ctx,
                           hpu::DloadFlag::small_bank);
    asm_code << hpu::psync();
    for (int component = 0; component < num_components; ++component) {
        asm_code << "        /* " << label << " component_" << component << " */\n";
        for (int i = 0; i < num_q; ++i) {
            asm_code << "        /* q_" << i << " */\n";
            asm_code << hpu::pmodld(i);
            asm_code << hpu::dload("x0", "x0", POBJ_POLY, hpu::DataType::poly);
            asm_code << generate_hpu_ntt_body_asm(N, POBJ_POLY, POBJ_TWIDDLE, false);
            asm_code << hpu::dstore("x0", "x0", POBJ_POLY, 1);
        }
    }
    asm_code << hpu::pfree(POBJ_MOD_CTX);

    return asm_code.str();
}

std::string generate_basis_intt_body_asm(
    int N,
    int num_q,
    int num_components,
    const std::string& label)
{
    std::ostringstream asm_code;

    const int POBJ_POLY = 0;
    const int POBJ_TWIDDLE = 3;
    const int POBJ_MOD_CTX = 4;

    asm_code << "        /* --- " << label << ": NTT -> coefficient domain --- */\n";
    asm_code << hpu::dload("x0", "x0", POBJ_MOD_CTX, hpu::DataType::mod_ctx,
                           hpu::DloadFlag::small_bank);
    asm_code << hpu::psync();
    for (int component = 0; component < num_components; ++component) {
        asm_code << "        /* " << label << " component_" << component << " */\n";
        for (int i = 0; i < num_q; ++i) {
            asm_code << "        /* q_" << i << " */\n";
            asm_code << hpu::pmodld(i);
            asm_code << hpu::dload("x0", "x0", POBJ_POLY, hpu::DataType::poly);
            asm_code << generate_hpu_intt_body_asm(N, POBJ_POLY, POBJ_TWIDDLE, false);
            asm_code << hpu::dstore("x0", "x0", POBJ_POLY, 1);
        }
    }
    asm_code << hpu::pfree(POBJ_MOD_CTX);

    return asm_code.str();
}

std::string generate_relinearize_t2_body_asm(
    int N,
    int num_q,
    int num_p,
    int dnum)
{
    std::ostringstream asm_code;

    const int total_bases = num_q + num_p;
    const int digit_size = num_q / dnum;

    const int POBJ_CT = 0;
    const int POBJ_EVK = 1;
    const int POBJ_OUT = 2;
    const int POBJ_TWIDDLE = 3;
    const int POBJ_MOD_CTX = 4;

    asm_code << "        /* --- Relinearization: KeySwitch(t2, rlk) -> (ks0, ks1) --- */\n";

    for (int d = 0; d < dnum; ++d) {
        const int q_offset = d * digit_size;
        asm_code << "        /* --- Relinearization digit " << d << " --- */\n";
        asm_code << "        /* Step 1: ModUp t2 digit Q_" << q_offset
                 << "..Q_" << (q_offset + digit_size - 1)
                 << " -> full Q union P */\n";
        asm_code << generate_hpu_hybrid_modup_body_asm(
            num_q,
            num_p,
            digit_size,
            q_offset,
            false);

        asm_code << "        /* Step 2: NTT on decomposed t2 digit over full Q union P */\n";
        asm_code << hpu::dload("x0", "x0", POBJ_MOD_CTX, hpu::DataType::mod_ctx,
                               hpu::DloadFlag::small_bank);
        asm_code << hpu::psync();
        for (int i = 0; i < total_bases; ++i) {
            asm_code << "        /* base_" << i << " */\n";
            asm_code << hpu::pmodld(i);
            asm_code << hpu::dload("x0", "x0", POBJ_CT, hpu::DataType::poly);
            asm_code << generate_hpu_ntt_body_asm(N, POBJ_CT, POBJ_TWIDDLE, false);
            asm_code << hpu::dstore("x0", "x0", POBJ_CT, 1);
        }

        asm_code << "        /* Step 3: multiply with relinearization key and accumulate digits */\n";
        for (int v = 0; v < 2; ++v) {
            asm_code << "        /* rlk component " << v << " */\n";
            for (int i = 0; i < total_bases; ++i) {
                asm_code << "        /* base_" << i << " */\n";
                asm_code << hpu::pmodld(i);
                asm_code << hpu::dload("x0", "x0", POBJ_CT, hpu::DataType::poly);
                asm_code << hpu::dload("x0", "x0", POBJ_EVK, hpu::DataType::poly);
                if (d == 0) {
                    asm_code << generate_hpu_mm_body_asm(POBJ_OUT, POBJ_CT, POBJ_EVK);
                } else {
                    asm_code << hpu::dload("x0", "x0", POBJ_OUT, hpu::DataType::poly);
                    asm_code << hpu::pmac(POBJ_OUT, POBJ_CT, POBJ_EVK);
                }
                asm_code << hpu::pfree(POBJ_CT);
                asm_code << hpu::pfree(POBJ_EVK);
                asm_code << hpu::dstore("x0", "x0", POBJ_OUT, 1);
            }
        }
        asm_code << hpu::pfree(POBJ_MOD_CTX);
    }

    asm_code << "        /* Step 4: INTT accumulated key-switch result over Q union P */\n";
    asm_code << hpu::dload("x0", "x0", POBJ_MOD_CTX, hpu::DataType::mod_ctx,
                           hpu::DloadFlag::small_bank);
    asm_code << hpu::psync();
    for (int v = 0; v < 2; ++v) {
        asm_code << "        /* ks component " << v << " */\n";
        for (int i = 0; i < total_bases; ++i) {
            asm_code << "        /* base_" << i << " */\n";
            asm_code << hpu::pmodld(i);
            asm_code << hpu::dload("x0", "x0", POBJ_CT, hpu::DataType::poly);
            asm_code << generate_hpu_intt_body_asm(N, POBJ_CT, POBJ_TWIDDLE, false);
            asm_code << hpu::dstore("x0", "x0", POBJ_CT, 1);
        }
    }
    asm_code << hpu::pfree(POBJ_MOD_CTX);

    asm_code << "        /* Step 5: ModDown each key-switch component Q union P -> Q */\n";
    for (int v = 0; v < 2; ++v) {
        asm_code << "        /* ModDown ks component " << v << " */\n";
        asm_code << generate_hpu_moddown_body_asm(num_q, num_p, false);
    }

    return asm_code.str();
}

std::string generate_add_relinearized_result_body_asm(int num_q)
{
    std::ostringstream asm_code;

    const int POBJ_LEFT = 0;
    const int POBJ_RIGHT = 1;
    const int POBJ_OUT = 2;
    const int POBJ_MOD_CTX = 4;

    asm_code << "        /* --- Compose final ciphertext: (t0, t1) + KeySwitch(t2) --- */\n";
    asm_code << hpu::dload("x0", "x0", POBJ_MOD_CTX, hpu::DataType::mod_ctx,
                           hpu::DloadFlag::small_bank);
    asm_code << hpu::psync();
    for (int v = 0; v < 2; ++v) {
        asm_code << "        /* final component " << v << " */\n";
        for (int i = 0; i < num_q; ++i) {
            asm_code << "        /* q_" << i << " */\n";
            asm_code << hpu::pmodld(i);
            asm_code << hpu::dload("x0", "x0", POBJ_LEFT, hpu::DataType::poly);
            asm_code << hpu::dload("x0", "x0", POBJ_RIGHT, hpu::DataType::poly);
            asm_code << hpu::padd(POBJ_OUT, POBJ_LEFT, POBJ_RIGHT);
            asm_code << hpu::pfree(POBJ_LEFT);
            asm_code << hpu::pfree(POBJ_RIGHT);
            asm_code << hpu::dstore("x0", "x0", POBJ_OUT, 1);
        }
    }
    asm_code << hpu::pfree(POBJ_MOD_CTX);

    return asm_code.str();
}

} // namespace

std::string generate_hpu_ciphertext_multiply_body_asm(
    int N,
    int num_q,
    int num_p,
    int dnum,
    bool append_psync)
{
    std::ostringstream asm_code;

    if (!is_valid_config(N, num_q, num_p, dnum)) {
        asm_code << "        // Invalid config: require power-of-two N, divisible digits, and at most 128 mod contexts\n";
        return asm_code.str();
    }

    asm_code << "        /* CIPHERTEXT MULTIPLY: ctA(2) * ctB(2) -> ctOut(2) with relinearization */\n";
    asm_code << generate_basis_ntt_body_asm(N, num_q, 2, "ctA");
    asm_code << generate_basis_ntt_body_asm(N, num_q, 2, "ctB");
    asm_code << "        /* --- Tensor product in NTT domain: (a0,a1)*(b0,b1)->(t0,t1,t2) --- */\n";
    asm_code << generate_hpu_cmult_body_asm(num_q, false);
    asm_code << generate_basis_intt_body_asm(N, num_q, 3, "tensor product");
    asm_code << generate_relinearize_t2_body_asm(N, num_q, num_p, dnum);
    asm_code << generate_add_relinearized_result_body_asm(num_q);

    if (append_psync) {
        asm_code << hpu::psync();
    }

    return asm_code.str();
}

std::string generate_hpu_ciphertext_multiply_asm(
    int N,
    int num_q,
    int num_p,
    int dnum,
    bool append_psync)
{
    std::ostringstream asm_code;
    asm_code << "void hpu_ciphertext_multiply_N" << N << "_Q" << num_q
             << "_P" << num_p << "_D" << dnum << "(void) {\n";

    if (!is_valid_config(N, num_q, num_p, dnum)) {
        asm_code << "    // Invalid config: require power-of-two N, divisible digits, and at most 128 mod contexts\n";
        asm_code << "}\n";
        return asm_code.str();
    }

    asm_code << "    __asm__ volatile(\n";
    asm_code << generate_hpu_ciphertext_multiply_body_asm(
        N,
        num_q,
        num_p,
        dnum,
        append_psync);
    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";

    return asm_code.str();
}
