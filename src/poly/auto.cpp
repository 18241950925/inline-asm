#include "poly/auto.hpp"
#include "poly/moddown.hpp"
#include "poly/modup.hpp"
#include "util/hpu_asm.hpp"
#include "util/mm.hpp"
#include "util/ntt.hpp"

#include <sstream>
#include <string>

namespace {
bool is_power_of_two(int x) { return x > 0 && (x & (x - 1)) == 0; }
} // namespace

std::string generate_hpu_auto_body_asm(
    int N,
    int num_q,
    int num_p,
    int dnum,
    int auto_idx,
    bool append_psync)
{
    std::ostringstream asm_code;

    if (num_q <= 0 || num_p <= 0 || !is_power_of_two(N) || dnum <= 0) return "";

    const int total_bases = num_q + num_p;
    const int digit_size = num_q / dnum;

	// 模数上下文在多项式槽位之后，且 q 基和 p 基分开存放
    const int MOD_CTX_Q_BASE = 0;
    const int MOD_CTX_P_BASE = num_q;
    auto mod_ctx_index_q = [&](int qi) { return MOD_CTX_Q_BASE + qi; };
    auto mod_ctx_index_p = [&](int pi) { return MOD_CTX_P_BASE + pi; };
    auto mod_ctx_index = [&](int base_idx) {
        return (base_idx < num_q) ? mod_ctx_index_q(base_idx) : mod_ctx_index_p(base_idx - num_q);
    };

    // ==========================================================
    // (仅使用 3 个多项式槽位, 1 个上下文槽位)
    // ==========================================================
    const int SLOT_A = 0;       // 通用工作槽 A
    const int SLOT_B = 1;       // 通用工作槽 B
    const int SLOT_C = 2;       // 累加器 / 输出槽
	const int TWIDDLE = 3;      
    const int POBJ_MOD_CTX = 4; // 模数上下文配置槽
	asm_code << hpu::dload("x0", "x0", POBJ_MOD_CTX, hpu::DataType::mod_ctx);

    asm_code << "        /* ========================================================== */\n";
    asm_code << "        /* --- Step 0: Manual Automorphism on c0 (NTT -> iNTT_auto)   */\n";
    asm_code << "        /* ========================================================== */\n";
    for (int i = 0; i < num_q; ++i) {
        asm_code << hpu::pmodld(POBJ_MOD_CTX, mod_ctx_index_q(i));
        // 从 HBM 加载 c0 的第 i 个通道到 SLOT_A
        asm_code << hpu::dload("x_c0", "x_offset", SLOT_A, hpu::DataType::poly);
        
		// 加载twiddle
		asm_code << hpu::dload("x0", "x0", TWIDDLE, hpu::DataType::poly);
        // NTT 转入频域
        asm_code << generate_hpu_ntt_body_asm(N, SLOT_A, TWIDDLE, POBJ_MOD_CTX, false);
        // 加载融合版twiddle
		asm_code << hpu::dload("x0", "x0", TWIDDLE, hpu::DataType::poly);
        // iNTT 融合旋转因子 (完成移位并退出频域)
        asm_code << generate_hpu_intt_body_asm(N, SLOT_A, TWIDDLE, POBJ_MOD_CTX, false);
        // 将旋转后的 c0 暂存回 HBM (开辟一块临时内存 "x_tmp_c0")
        asm_code << hpu::dstore("x_tmp_c0", "x_offset", SLOT_A, 1); 
    }

    asm_code << "\n        /* ========================================================== */\n";
    asm_code << "        /* --- Step 1..5: Keyswitch on ct1 (Fused Auto in NTT)        */\n";
    asm_code << "        /* ========================================================== */\n";
    for (int d = 0; d < dnum; ++d) {
        const int q_offset = d * digit_size;
        
        asm_code << "        /* --- Step 1: ModUp on ct1 --- */\n";
        // ModUp 会将数据从 HBM 读出，拉伸后写回 HBM 的 "x_ct1_up"
        asm_code << generate_hpu_modup_body_asm(digit_size, num_p, q_offset, false);

        asm_code << "        /* --- Step 2: Fused NTT Auto on Q and P bases --- */\n";
        for (int i = 0; i < total_bases; ++i) {
            asm_code << hpu::pmodld(POBJ_MOD_CTX, mod_ctx_index(i));
            // 读入升模后的切片
            asm_code << hpu::dload("x_ct1_up", "x_offset", SLOT_A, hpu::DataType::poly);
			// 加载twiddle
			asm_code << hpu::dload("x0", "x0", TWIDDLE, hpu::DataType::poly);
            // 执行融合了 auto_idx 的 NTT
            asm_code << generate_hpu_ntt_body_asm(N, SLOT_A, TWIDDLE, POBJ_MOD_CTX, false);
            // 将处于求值域(且已位移)的碎片写回 HBM "x_ct1_ntt"
            asm_code << hpu::dstore("x_ct1_ntt", "x_offset", SLOT_A, 1);
        }

        asm_code << "        /* --- Step 3: Multiply and Accumulate with EVK --- */\n";
        for (int v = 0; v < 2; ++v) {
            for (int i = 0; i < total_bases; ++i) {
                asm_code << hpu::pmodld(POBJ_MOD_CTX, mod_ctx_index(i));
                
                // SLOT_A 加载碎片; SLOT_B 加载评估密钥
                asm_code << hpu::dload("x_ct1_ntt", "x_offset", SLOT_A, hpu::DataType::poly);
                asm_code << hpu::dload("x_evk", "x_offset", SLOT_B, hpu::DataType::poly); // 指针需随 v 偏移
                
                if (d == 0) {
                    asm_code << hpu::pmul(SLOT_C, SLOT_A, SLOT_B); // 第一次直接相乘存入 SLOT_C
                } else {
                    // 非第一次，需要把之前的累加结果从 HBM 读到 SLOT_C
                    asm_code << hpu::dload("x_out", "x_offset", SLOT_C, hpu::DataType::poly);
                    asm_code << hpu::pmac(SLOT_C, SLOT_A, SLOT_B); // MAC 累加
                }
                // 立即将累加结果写回 HBM，释放 SRAM！
                asm_code << hpu::dstore("x_out", "x_offset", SLOT_C, 1);
            }
        }
    }

    asm_code << "\n        /* --- Step 4: INTT on the keyed outputs --- */\n";
    for (int v = 0; v < 2; ++v) {
        for (int i = 0; i < total_bases; ++i) {
            asm_code << hpu::pmodld(POBJ_MOD_CTX, mod_ctx_index(i));
            asm_code << hpu::dload("x_out", "x_offset", SLOT_A, hpu::DataType::poly);
            
			// 加载twiddle
			asm_code << hpu::dload("x0", "x0", TWIDDLE, hpu::DataType::poly);
            // 正常 iNTT 退出频域 (auto_idx = 0)
            asm_code << generate_hpu_intt_body_asm(N, SLOT_A, TWIDDLE, POBJ_MOD_CTX, false);
            asm_code << hpu::dstore("x_out", "x_offset", SLOT_A, 1);
        }
    }

    asm_code << "\n        /* --- Step 5: ModDown --- */\n";
    for (int v = 0; v < 2; ++v) {
        // ModDown 流式处理，结果覆盖写回 "x_out"
        asm_code << generate_hpu_moddown_body_asm(num_q, num_p, false);
    }

    asm_code << "\n        /* --- Step 6: Final Merge (c0 + out0) --- */\n";
    for (int i = 0; i < num_q; ++i) {
        asm_code << hpu::pmodld(POBJ_MOD_CTX, mod_ctx_index_q(i));
        // SLOT_A 读取 Step 5 降模后的 out0
        asm_code << hpu::dload("x_out", "x_offset", SLOT_A, hpu::DataType::poly);
        // SLOT_B 读取 Step 0 存在临时区的旋转后的 c0
        asm_code << hpu::dload("x_tmp_c0", "x_offset", SLOT_B, hpu::DataType::poly); 
        
        // 在 SLOT_C 中相加
        asm_code << hpu::padd(SLOT_C, SLOT_A, SLOT_B);
        
        // 最终合规的新密文 $c_0'$ 写回主存
        asm_code << hpu::dstore("x_out", "x_offset", SLOT_C, 1);
    }

    if (append_psync) asm_code << hpu::psync(0);

    return asm_code.str();
}

std::string generate_hpu_auto_asm(
	int N,
	int num_q,
	int num_p,
	int dnum,
	int auto_idx,
	bool append_psync)
{
	std::ostringstream asm_code;
	asm_code << "void hpu_auto_N" << N << "_Q" << num_q << "_P" << num_p << "_D" << dnum
			 << "_A" << auto_idx << "(void) {\n";

	if (num_q <= 0 || num_p <= 0 || !is_power_of_two(N) || dnum <= 0) {
		asm_code << "    // Invalid config: require num_q/num_p/dnum > 0 and power-of-two N\n";
		asm_code << "}\n";
		return asm_code.str();
	}

	asm_code << "    __asm__ volatile(\n";
	asm_code << "        /* AUTO: shuffle ct0/ct1, then keyswitch ct1 and fold ct0 into out0 */\n";
	asm_code << generate_hpu_auto_body_asm(N, num_q, num_p, dnum, auto_idx, append_psync);
	asm_code << "        : \n";
	asm_code << "        : \n";
	asm_code << "        : \"memory\"\n";
	asm_code << "    );\n";
	asm_code << "}\n";

	return asm_code.str();
}
