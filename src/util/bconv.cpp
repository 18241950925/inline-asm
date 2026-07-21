#include "util/bconv.hpp"
#include "util/hpu_asm.hpp"
#include <sstream>
#include <string>
#include <vector>

namespace {

bool valid_contexts(const std::vector<int>& contexts)
{
    for (int context : contexts) {
        if (context < 0 || context >= hpu::kMaxModContexts) {
            return false;
        }
    }
    return true;
}

} // namespace

std::string generate_hpu_bconv_contexts_body_asm(
    const std::vector<int>& source_contexts,
    const std::vector<int>& target_contexts,
    bool append_psync)
{
    std::ostringstream asm_code;

    if (source_contexts.empty() || target_contexts.empty()
        || !valid_contexts(source_contexts) || !valid_contexts(target_contexts)) {
        asm_code << "        // Invalid config: BConv requires non-empty 8-bit MOD_ID values\n";
        return asm_code.str();
    }

    // 根据 HPU 硬件限制，只能存入 3 个多项式对象
    // 我们将复用固定的 3 个寄存器槽位：0, 1, 2
    const int POBJ_TMP_A = 0;
    const int POBJ_TMP_B = 1;
    const int POBJ_ACC   = 2;
    // type=mod_ctx 的 dload 传输句柄；pmodld 通过固定模表的 MOD_ID 选项
    const int POBJ_MOD_CTX = 4;

    // ==========================================
    // 阶段一：预处理 (Pre-multiply)
    // 对每个输入基 b_j 计算: x_j = [a_j * b_hat_inv] mod b_j
    // ==========================================
    // 一次性安装固定模表；POBJ_MOD_CTX 仅作为 custom1 DMA 传输句柄
    asm_code << "        // dload all mod contexts (placeholder)\n";
    asm_code << hpu::dload("x0", "x0", POBJ_MOD_CTX, hpu::DataType::mod_ctx);

    asm_code << "        /* --- STAGE 1: Precompute in source basis --- */\n";
    for (std::size_t j = 0; j < source_contexts.size(); ++j) {
        asm_code << "        /* Source context " << source_contexts[j]
                 << ", source limb " << j << " */\n";
        asm_code << hpu::pmodld(source_contexts[j]);

        asm_code << "        // dload a_j and qhat_inv_j (placeholder)\n";
        asm_code << hpu::dload("x0", "x0", POBJ_TMP_A, hpu::DataType::poly);
        asm_code << hpu::dload("x0", "x0", POBJ_TMP_B, hpu::DataType::poly);

        asm_code << hpu::pmul(POBJ_TMP_A, POBJ_TMP_A, POBJ_TMP_B);
        asm_code << hpu::pfree(POBJ_TMP_B);

        asm_code << "        // dstore x_j to tmp memory (placeholder)\n";
        asm_code << hpu::dstore("x0", "x0", POBJ_TMP_A, 1);
    }

    // ==========================================
    // 阶段二：跨基累加 (Accumulate)
    // 对每个目标基 t_i 计算: Acc_i = SUM(x_j * b_hat_j) mod t_i
    // ==========================================
    asm_code << "        /* --- STAGE 2: Accumulate in target basis --- */\n";
    for (std::size_t i = 0; i < target_contexts.size(); ++i) {
        asm_code << "        /* Target context " << target_contexts[i]
                 << ", target limb " << i << " */\n";
        asm_code << hpu::pmodld(target_contexts[i]);

        for (std::size_t j = 0; j < source_contexts.size(); ++j) {
            asm_code << "        // dload x_j and qhat_modp_j_i (placeholder)\n";
            asm_code << hpu::dload("x0", "x0", POBJ_TMP_A, hpu::DataType::poly);
            asm_code << hpu::dload("x0", "x0", POBJ_TMP_B, hpu::DataType::poly);

            if (j == 0) {
                asm_code << hpu::pmul(POBJ_ACC, POBJ_TMP_A, POBJ_TMP_B);
            } else {
                asm_code << hpu::pmac(POBJ_ACC, POBJ_TMP_A, POBJ_TMP_B);
            }
            asm_code << hpu::pfree(POBJ_TMP_A);
            asm_code << hpu::pfree(POBJ_TMP_B);
        }
        
        asm_code << "        // dstore Acc_i to target memory (placeholder)\n";
        asm_code << hpu::dstore("x0", "x0", POBJ_ACC, 1);
    }

    asm_code << hpu::pfree(POBJ_MOD_CTX);

    if (append_psync) {
        asm_code << hpu::psync(0);
    }

    return asm_code.str();
}

std::string generate_hpu_bconv_body_asm(
    int num_q,
    int num_p,
    int q_offset,
    bool append_psync)
{
    if (num_q <= 0 || num_p <= 0 || q_offset < 0
        || q_offset + num_q + num_p > hpu::kMaxModContexts) {
        return "        // Invalid config: require positive bases within 8-bit MOD_ID space\n";
    }

    std::vector<int> source_contexts;
    std::vector<int> target_contexts;
    source_contexts.reserve(static_cast<std::size_t>(num_q));
    target_contexts.reserve(static_cast<std::size_t>(num_p));
    for (int i = 0; i < num_q; ++i) {
        source_contexts.push_back(q_offset + i);
    }
    for (int i = 0; i < num_p; ++i) {
        target_contexts.push_back(q_offset + num_q + i);
    }
    return generate_hpu_bconv_contexts_body_asm(
        source_contexts,
        target_contexts,
        append_psync);
}

std::string generate_hpu_bconv_asm(
    int num_q,
    int num_p,
    int q_offset,
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
        q_offset,
        append_psync);

    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";

    return asm_code.str();
}
