#include "ntt.hpp"

#include <cmath>
#include <stdint.h>
#include <sstream>
#include <string>

// HPU CG-NTT (Constant Geometry NTT) 汇编代码生成函数
/**
 * @brief 生成 HPU (Heterogeneous Processing Unit) 上的 CG-NTT (Constant Geometry NTT) 内联汇编代码
 *
 * 此函数用于动态生成执行恒定几何 NTT 算法的 C 语言内联汇编字符串。
 * 它采用 Ping-Pong 缓冲机制处理各级 (Stage) 的数据迭代，并使用 HPU 的专用计算与洗牌指令
 * (如 pmodsw, ptwld, ptwid, pntt, pshuf2)。
 *
 * @param N             NTT 变换的整体长度 (点数)。必须是 2 的幂（例如 256, 1024, 4096 等）。
 * 该参数决定了外层 Stage 的迭代次数 (log2(N))。
 * @param l             硬件向量化处理的长度 (Vector Length)。
 * 表示单条 SIMD 指令或单个硬件槽位能并行处理的元素个数。
 * 用于计算内层循环的向量块总数 (num_vecs = N / l)。
 * @param base_addr_in  输入数据在 HPU 本地存储 (如 SRAM 或 TCDM) 中的起始基地址。
 * 在 Ping-Pong 结构中，作为初始的“读”地址空间。
 * @param base_addr_out 辅助缓冲区的起始基地址。要求与输入缓冲区大小一致且不重叠。
 * 在 Ping-Pong 结构中，作为初始的“写”地址空间。
 * 注意：根据 log2(N) 的奇偶性，最终的 NTT 结果可能驻留在 base_addr_in 
 * 或 base_addr_out 中。
 * @param mod_id        HPU 硬件中预配置的多项式环模数槽位 ID (Modulo ID)。
 * 用于 `pmodsw` 指令。该值必须严格遵循 HPU 硬件规范（例如 0~3），
 * 硬件会根据此 ID 自动加载对应的模数 $q$ 进行模加、模乘运算。
 *
 * @return std::string  返回包含完整 void hpu_cgntt_N{N}_l{l}(void) 函数定义的源码字符串，
 * 可以直接输出到 .c / .cpp 文件中参与后续的交叉编译。
 */
std::string generate_hpu_ntt_asm(int N, int l, uint16_t base_addr_in, uint16_t base_addr_out, int mod_id) {
    std::ostringstream asm_code;
    
    int logN = std::log2(N);
    int num_vecs = N / l; 
    
    // 假设硬件预定义了一个用于 CG-NTT 的 Perfect Shuffle 模板 ID
    int shf_id_perfect = 0x00; 
    
    asm_code << "void hpu_ntt_N" << N << "_l" << l << "(void) {\n";
    asm_code << "    __asm__ volatile(\n";
    
    // 初始化上下文：模数槽与洗牌配置在整个 NTT 过程中不再改变！
    asm_code << "        \"pmodsw " << mod_id << " \\n\\t\"\n";
    asm_code << "        \"pshcfg " << shf_id_perfect << " \\n\\t\"\n";
    
    // Ping-Pong Buffer 地址翻转逻辑
    // 因为完美洗牌会导致数据位置整体移动，通常需要两块大小相等的 SRAM 区域交替写回
    uint16_t addr_src = base_addr_in;
    uint16_t addr_dst = base_addr_out;

    for (int stage = 1; stage <= logN; stage++) {
        asm_code << "\n        // ==========================================\n";
        asm_code << "        // Stage " << stage << " (Constant Geometry)\n";
        asm_code << "        // ==========================================\n";
        
        
        
        // 核心亮点：没有 Stride 分支！恒定读取前一半和后一半的数据
        int half_vecs = num_vecs / 2; 
        
        for (int i = 0; i < half_vecs; i++) {

            // 在 CG-NTT 中，取数的逻辑距离总是 N/2
            uint16_t addr_x = addr_src + i;
            uint16_t addr_y = addr_src + i + half_vecs;
            
            // 写入的目标地址由 Perfect Shuffle 决定（通常是交错写入 2i 和 2i+1）
            uint16_t addr_out_x = addr_dst + (i * 2);
            uint16_t addr_out_y = addr_dst + (i * 2) + 1;
            
            asm_code << "        /* Vector Block " << i << " */\n";
            asm_code << "        \"ptwld " << i << " \\n\\t\"\n";
            asm_code << "        \"sload " << addr_x << ", p0 \\n\\t\"\n";
            asm_code << "        \"sload " << addr_y << ", p1 \\n\\t\"\n";
            
            // 1. 执行蝶形运算，隐式覆盖 p0, p1
            asm_code << "        \"pntt p0, p1, p0 \\n\\t\"\n";
            
            
            // 2. 执行统一的完美洗牌，重排数据分布，覆盖 p0, p1
            asm_code << "        \"pshuf2 p0, p1, p0 \\n\\t\"\n";
            
            // 3. 按序写回 Ping-Pong 缓冲区的目标地址
            asm_code << "        \"sstore p0, " << addr_out_x << " \\n\\t\"\n";
            asm_code << "        \"sstore p1, " << addr_out_y << " \\n\\t\"\n";
        }
        
        // Stage 结束，翻转 Ping-Pong Buffer 的指针
        std::swap(addr_src, addr_dst);
        asm_code << "        \"ptwid \\n\\t\"\n";
    }
    
    // 如果 logN 是奇数，最终结果可能在 addr_dst 中，需要根据情况处理搬运
    asm_code << "\n        // 结束\n";
    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";
    
    return asm_code.str();
}