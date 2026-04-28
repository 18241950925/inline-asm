#include "util/bconv.hpp"
#include "util/hpu_asm.hpp"
#include <sstream>
#include <string>

std::string generate_hpu_bconv_body_asm(
    int num_q,
    int num_p,
    bool append_psync)
{
    std::ostringstream asm_code;

    // 根据 HPU 硬件限制，只能存入 3 个多项式对象
    // 我们将复用固定的 3 个寄存器槽位：0, 1, 2
    const int POBJ_TMP_A = 0;
    const int POBJ_TMP_B = 1;
    const int POBJ_ACC   = 2;
    // 模上下文对象槽位
    const int POBJ_MOD_CTX = 4;

    // ==========================================
    // 阶段一：预处理 (Pre-multiply)
    // 对每个输入基 q_j 计算: x_j = [a_j * q_hat_inv] mod q_j
    // ==========================================
    // 一次性读取所有的模上下文到对象 POBJ_MOD_CTX 中
    asm_code << "        // dload all mod contexts (placeholder)\n";
    asm_code << hpu::dload("x0", "x0", POBJ_MOD_CTX, hpu::DataType::mod_ctx);

    asm_code << "        /* --- STAGE 1: Precompute in Input Basis Q --- */\n";
    for (int j = 0; j < num_q; ++j) {
        asm_code << "        /* Context: q_" << j << " */\n";
        // 使用 pmodld 的第二个参数作为识别 id 从对象中读取不同的模上下文
        asm_code << hpu::pmodld(POBJ_MOD_CTX, j);

        asm_code << "        // dload q_j and qhat_inv_j (placeholder)\n";
        asm_code << hpu::dload("x0", "x0", POBJ_TMP_A, hpu::DataType::poly);
        asm_code << hpu::dload("x0", "x0", POBJ_TMP_B, hpu::DataType::poly);

        asm_code << hpu::pmul(POBJ_TMP_A, POBJ_TMP_A, POBJ_TMP_B);

        asm_code << "        // dstore x_j to tmp memory (placeholder)\n";
        asm_code << hpu::dstore("x0", "x0", POBJ_TMP_A, 0);
    }

    // ==========================================
    // 阶段二：跨基累加 (Accumulate)
    // 对每个目标基 p_i 计算: Acc_i = SUM( x_j * q_hat_j ) mod p_i
    // ==========================================
    asm_code << "        /* --- STAGE 2: Accumulate in Target Basis P --- */\n";
    for (int i = 0; i < num_p; ++i) {
        asm_code << "        /* Context: p_" << i << " */\n";
        // 假设 Q 和 P 的上下文都在同一个 dload 中或者 DMA 给的是完整连续区间
        // 或者是 pmodld 的第二个参数延续编号，这里假设 id 接着 num_q，或者是单独的 0~num_p
        // 我们传入 i，但如果是同一份文件或者连续地址，由于这是独立编译的 UT ASM，使用对应 id
        asm_code << hpu::pmodld(POBJ_MOD_CTX, num_q + i);

        for (int j = 0; j < num_q; ++j) {
            asm_code << "        // dload x_j and qhat_modp_j_i (placeholder)\n";
            asm_code << hpu::dload("x0", "x0", POBJ_TMP_A, hpu::DataType::poly);
            asm_code << hpu::dload("x0", "x0", POBJ_TMP_B, hpu::DataType::poly);

            if (j == 0) {
                asm_code << hpu::pmul(POBJ_ACC, POBJ_TMP_A, POBJ_TMP_B);
            } else {
                asm_code << hpu::pmac(POBJ_ACC, POBJ_TMP_A, POBJ_TMP_B);
            }
        }
        
        asm_code << "        // dstore Acc_i to target memory (placeholder)\n";
        asm_code << hpu::dstore("x0", "x0", POBJ_ACC, 0);
    }

    if (append_psync) {
        asm_code << hpu::psync(0);
    }

    return asm_code.str();
}

std::string generate_hpu_bconv_asm(
    int num_q,
    int num_p,
    bool append_psync)
{
    std::ostringstream asm_code;
    asm_code << "void hpu_full_bconv_Q" << num_q << "_P" << num_p << "(void) {\n";

    if (num_q <= 0 || num_p <= 0) {
        asm_code << "    // Invalid config: require num_q/num_p > 0\n";
        asm_code << "}\n";
        return asm_code.str();
    }

    asm_code << "    __asm__ volatile(\n";
    asm_code << generate_hpu_bconv_body_asm(
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
