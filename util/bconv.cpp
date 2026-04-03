#include "bconv.hpp"
#include <sstream>
#include <string>

/**
 * Basis Conversion HPU 汇编生成器
 * @param N 向量长度 (多项式度数)
 * @param l 硬件向量寄存器的 Lane 数量
 * @param num_q 输入基的个数 (L)
 * @param num_p 目标基的个数 (K)
 * @param base_addr_in 输入数据 a_j 在 SRAM 的起始地址
 * @param base_addr_tmp 阶段一预处理结果 x_j 在 SRAM 的暂存地址
 * @param base_addr_out 最终结果 Acc_i 在 SRAM 的起始地址
 */
std::string generate_hpu_bconv_asm(
    int N, int l, 
    int num_q, int num_p,
    uint16_t base_addr_in, 
    uint16_t base_addr_tmp, 
    uint16_t base_addr_out) 
{
    std::ostringstream asm_code;
    int num_vecs = N / l;

    asm_code << "void hpu_full_bconv_N" << N << "_Q" << num_q << "_P" << num_p << "(void) {\n";
    asm_code << "    __asm__ volatile(\n";

    // ==========================================
    // 阶段一：预处理 (Pre-multiply)
    // 对每个输入基 q_j 计算: x_j = [a_j * q_hat_inv] mod q_j
    // ==========================================
    asm_code << "        /* --- STAGE 1: Precompute in Input Basis Q --- */\n";
    for (int j = 0; j < num_q; ++j) {
        int mod_slot_q = j % 4; // 假设模数槽循环使用
        int cst_id_inv = 0;     // 假设 q_hat_inv 存在常量寄存器 c0 (注：实际需硬件支持装载)
        
        asm_code << "        /* Context: q_" << j << " */\n";
        asm_code << "        \"pmodsw " << mod_slot_q << " \\n\\t\"\n";

        for (int v = 0; v < num_vecs; ++v) {
            uint16_t addr_in = base_addr_in + (j * num_vecs) + v;
            uint16_t addr_tmp = base_addr_tmp + (j * num_vecs) + v;

            asm_code << "        \"sload " << addr_in << ", p0 \\n\\t\"\n";
            asm_code << "        \"pbcast c" << cst_id_inv << ", p1 \\n\\t\"\n";
            asm_code << "        \"pmul p0, p1, p2 \\n\\t\"\n"; // 仅乘法
            asm_code << "        \"sstore p2, " << addr_tmp << " \\n\\t\"\n";
        }
    }

    // ==========================================
    // 阶段二：跨基累加 (Accumulate)
    // 对每个目标基 p_i 计算: Acc_i = SUM( x_j * q_hat_j ) mod p_i
    // ==========================================
    asm_code << "        /* --- STAGE 2: Accumulate in Target Basis P --- */\n";
    for (int i = 0; i < num_p; ++i) {
        int mod_slot_p = (num_q + i) % 4; // 假设目标模数存在其他槽位
        
        asm_code << "        /* Context: p_" << i << " */\n";
        asm_code << "        \"pmodsw " << mod_slot_p << " \\n\\t\"\n";

        for (int v = 0; v < num_vecs; ++v) {
            uint16_t addr_out = base_addr_out + (i * num_vecs) + v;
            
            // 目标基下，每个向量块的累加循环
            for (int j = 0; j < num_q; ++j) {
                uint16_t addr_tmp = base_addr_tmp + (j * num_vecs) + v;
                int cst_id_qhat = 1; // 假设当前 q_hat_j(mod p_i) 存在常量寄存器 c1

                asm_code << "        \"sload " << addr_tmp << ", p0 \\n\\t\" /* load x_" << j << " */\n";
                asm_code << "        \"pbcast c" << cst_id_qhat << ", p1 \\n\\t\"\n";
                
                if (j == 0) {
                    // 第一次累加，累加器 p2 初始为当前 x_0 * q_hat_0
                    asm_code << "        \"pmul p0, p1, p2 \\n\\t\"\n";
                } else {
                    // 后续使用 pmac 累加到 p2: p2 = p2 + p0 * p1
                    asm_code << "        \"pmac p0, p1, p2 \\n\\t\"\n";
                }
            }
            // 将整个 q_j 循环累加完的结果写回输出地址
            asm_code << "        \"sstore p2, " << addr_out << " \\n\\t\"\n";
        }
    }

    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";

    return asm_code.str();
}