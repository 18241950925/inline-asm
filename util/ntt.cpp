#include "ntt.hpp"

#include <cmath>
#include <stdint.h>
#include <sstream>
#include <string>

// FHE NTT 汇编代码生成函数
// N: 多项式阶数 (如 4096)
// l: 向量寄存器长度 (如 16)
// base_addr: HPU SRAM 基址分配 (如 0x0000)
// mod_id: 模数上下文槽位 ID
std::string generate_hpu_ntt_asm(int N, int l, uint16_t base_addr, int mod_id) {
    std::ostringstream asm_code;
    
    int logN = std::log2(N);
    int num_vecs = N / l; // 总共需要的向量块数量
    
    // 函数头声明
    asm_code << "void hpu_ntt_complete_N" << N << "_l" << l << "(void) {\n";
    asm_code << "    __asm__ volatile(\n";
    
    int twiddle_id = 0; // 记录 Twiddle 状态的递增
    
    // 外层循环：对应 SEAL 中的 logN 个 Stage，步长(stride)从 N/2 逐渐减半到 1
    for (int stage = 1; stage <= logN; stage++) {
        int stride = N >> stage;       // 蝶形运算的物理距离
        int num_blocks = 1 << (stage - 1); // 当前 Stage 包含的独立计算块数量
        
        asm_code << "\n        // ==========================================\n";
        asm_code << "        // Stage " << stage << ": Stride = " << stride << "\n";
        asm_code << "        // ==========================================\n";
        
        // 每个 Stage 初始化模上下文和当前的 Twiddle 起点
        asm_code << "        \"pmodsw " << mod_id << " \\n\\t\"\n";
        asm_code << "        \"ptwld " << twiddle_id << " \\n\\t\"\n";
        
        if (stride >= l) {
            // ==========================================
            // 分支 1：跨向量运算 (Stride >= l)
            // 蝶形运算的两个输入在不同的 SRAM 块中，无需 Shuffle
            // ==========================================
            int vec_stride = stride / l; // 转换为按向量地址计算的步长
            
            for (int block = 0; block < num_blocks; block++) {
                int block_start_vec = block * 2 * vec_stride;
                
                // 遍历当前块内的所有向量
                for (int j = 0; j < vec_stride; j++) {
                    uint16_t addr_x = base_addr + block_start_vec + j;
                    uint16_t addr_y = addr_x + vec_stride;
                    
                    asm_code << "        /* Block " << block << ", Offset " << j << " */\n";
                    asm_code << "        \"sload " << addr_x << ", p0 \\n\\t\"\n";
                    asm_code << "        \"sload " << addr_y << ", p1 \\n\\t\"\n";
                    asm_code << "        \"pntt p0, p1, p2 \\n\\t\"\n";
                    asm_code << "        \"ptwid \\n\\t\"\n";
                    asm_code << "        \"sstore p0, " << addr_x << " \\n\\t\"\n";
                    asm_code << "        \"sstore p1, " << addr_y << " \\n\\t\"\n";
                }
            }
        } else {
            // ==========================================
            // 分支 2：向量内运算 (Stride < l)
            // 蝶形运算的物理距离小于 l，必须利用 pshuf2 进行内部混洗
            // ==========================================
            // 假设依据 stride 计算出了一个 shuffle config ID
            // 实际硬件中这里可能是固定的映射关系，比如 stride=8 对应 0xA
            int shf_id = 0xA + (l / stride); 
            
            asm_code << "        \"pshcfg " << shf_id << " \\n\\t\"\n";
            
            // 因为在向量内部运算，我们每次仍然取相邻的两个块丢进去洗牌计算
            // 每次提取 2 个向量，共需循环 (num_vecs / 2) 次
            for (int i = 0; i < num_vecs / 2; i++) {
                uint16_t addr_x = base_addr + i * 2;
                uint16_t addr_y = base_addr + i * 2 + 1;
                
                asm_code << "        \"sload " << addr_x << ", p0 \\n\\t\"\n";
                asm_code << "        \"sload " << addr_y << ", p1 \\n\\t\"\n";
                asm_code << "        \"pshuf2 p0, p1, p2 \\n\\t\"\n";
                asm_code << "        \"pntt p0, p1, p2 \\n\\t\"\n";
                asm_code << "        \"ptwid \\n\\t\"\n";
                // 如果需要逆向洗牌恢复顺序，可以在这里增加指令
                asm_code << "        \"sstore p0, " << addr_x << " \\n\\t\"\n";
                asm_code << "        \"sstore p1, " << addr_y << " \\n\\t\"\n";
            }
        }
        twiddle_id++; // 准备下一个 Stage 的 Twiddle
    }
    
    // 尾部收尾，并假设最后一条指令触发中断 IE=1（可选逻辑）
    asm_code << "\n        // 结束并通知 RISC-V\n";
    asm_code << "        : \n";
    asm_code << "        : \n";
    asm_code << "        : \"memory\"\n";
    asm_code << "    );\n";
    asm_code << "}\n";
    
    return asm_code.str();
}