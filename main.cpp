#include <iostream>
#include <string>

#include "util/bconv.hpp"
#include "util/mm.hpp"
#include "util/ntt.hpp"

namespace {

struct NttConfig {
	int N;
	int obj_poly_a;
	int obj_poly_b;
	int mod_ctx_obj;
	int shf_cfg_obj;
};

struct MmConfig {
	int obj_a;
	int obj_b;
	int obj_c;
	int mod_ctx_obj;
};

struct BconvConfig {
	int num_q;
	int num_p;
	int obj_q_base;
	int obj_tmp_base;
	int obj_p_base;
	int obj_qhat_inv_base;
	int obj_qhat_modp_base;
	int mod_ctx_q_base;
	int mod_ctx_p_base;
};

constexpr NttConfig kNttCfg{64, 0, 1, 2, 3};
constexpr MmConfig kMmCfg{0, 1, 2, 3};
// 为了演示 3-bit 槽位约束，示例采用 num_q = num_p = 1
constexpr BconvConfig kBconvCfg{1, 1, 0, 1, 2, 3, 4, 5, 6};

void test_ntt_codegen()
{
	std::string ntt = generate_hpu_ntt_asm(
		kNttCfg.N,
		kNttCfg.obj_poly_a,
		kNttCfg.obj_poly_b,
		kNttCfg.mod_ctx_obj,
		kNttCfg.shf_cfg_obj);
	std::cout << "===== NTT ASM =====\n" << ntt << "\n";
}

void test_mm_codegen()
{
	std::string mm = generate_hpu_mm_asm(
		kMmCfg.obj_a,
		kMmCfg.obj_b,
		kMmCfg.obj_c,
		kMmCfg.mod_ctx_obj);
	std::cout << "===== MM ASM =====\n" << mm << "\n";
}

void test_bconv_codegen()
{
	std::string bconv = generate_hpu_bconv_asm(
		kBconvCfg.num_q,
		kBconvCfg.num_p,
		kBconvCfg.obj_q_base,
		kBconvCfg.obj_tmp_base,
		kBconvCfg.obj_p_base,
		kBconvCfg.obj_qhat_inv_base,
		kBconvCfg.obj_qhat_modp_base,
		kBconvCfg.mod_ctx_q_base,
		kBconvCfg.mod_ctx_p_base);
	std::cout << "===== BCONV ASM =====\n" << bconv << "\n";
}

} // namespace

int main()
{
	test_ntt_codegen();
	test_mm_codegen();
	test_bconv_codegen();
	return 0;
}
