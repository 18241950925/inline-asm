#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "util/bconv.hpp"
#include "util/mm.hpp"
#include "util/ntt.hpp"
#include "poly/cmult.hpp"
#include "poly/moddown.hpp"
#include "poly/modup.hpp"
#include "poly/pmult.hpp"
#include "operator/keyswitch.hpp"

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

struct PmultConfig {
	int num_q;
	int ct0_base;
	int ct1_base;
	int pt_base;
	int out0_base;
	int out1_base;
	int mod_ctx_q_base;
};

struct CmultConfig {
	int num_q;
	int a0_base;
	int a1_base;
	int b0_base;
	int b1_base;
	int out0_base;
	int out1_base;
	int out2_base;
	int mod_ctx_q_base;
};

struct ModdownConfig {
	int num_q;
	int num_p;
	int q_base;
	int p_base;
	int tmp_base;
	int qcorr_base;
	int phat_inv_base;
	int phat_modq_base;
	int mod_ctx_p_base;
	int mod_ctx_q_base;
};

constexpr NttConfig kNttCfg{64, 0, 1, 2, 3};
constexpr MmConfig kMmCfg{0, 1, 2, 3};
// 为了演示 3-bit 槽位约束，示例采用 num_q = num_p = 1
constexpr BconvConfig kBconvCfg{1, 1, 0, 1, 2, 3, 4, 5, 6};
constexpr PmultConfig kPmultCfg{1, 0, 1, 2, 3, 4, 5};
constexpr CmultConfig kCmultCfg{1, 0, 1, 2, 3, 4, 5, 6, 7};
constexpr ModdownConfig kModdownCfg{1, 1, 0, 1, 2, 3, 4, 5, 6, 7};

void test_intt_codegen() {
	std::string intt = generate_hpu_intt_asm(
		kNttCfg.N,
		kNttCfg.obj_poly_a,
		kNttCfg.obj_poly_b,
		kNttCfg.mod_ctx_obj,
		kNttCfg.shf_cfg_obj);
	std::ofstream("output/intt.cpp") << intt;
	std::cout << "Saved intt ASM to output/intt.cpp\n";
}

void test_ntt_codegen()
{
	std::string ntt = generate_hpu_ntt_asm(
		kNttCfg.N,
		kNttCfg.obj_poly_a,
		kNttCfg.obj_poly_b,
		kNttCfg.mod_ctx_obj,
		kNttCfg.shf_cfg_obj);
	std::ofstream("output/ntt.cpp") << ntt;
	std::cout << "Saved ntt ASM to output/ntt.cpp\n";
}

void test_mm_codegen()
{
	std::string mm = generate_hpu_mm_asm(
		kMmCfg.obj_a,
		kMmCfg.obj_b,
		kMmCfg.obj_c,
		kMmCfg.mod_ctx_obj);
	std::ofstream("output/mm.cpp") << mm;
	std::cout << "Saved mm ASM to output/mm.cpp\n";
}

void test_bconv_codegen()
{
	std::string bconv = generate_hpu_bconv_asm(
		kBconvCfg.num_q,
		kBconvCfg.num_p);
	std::ofstream("output/bconv.cpp") << bconv;
	std::cout << "Saved bconv ASM to output/bconv.cpp\n";
}

void test_pmult_codegen()
{
	std::string pmult = generate_hpu_pmult_asm(
		kPmultCfg.num_q,
		true);
	std::ofstream("output/pmult.cpp") << pmult;
	std::cout << "Saved pmult ASM to output/pmult.cpp\n";
}

void test_cmult_codegen()
{
	std::string cmult = generate_hpu_cmult_asm(
		kCmultCfg.num_q,
		true);
	std::ofstream("output/cmult.cpp") << cmult;
	std::cout << "Saved cmult ASM to output/cmult.cpp\n";
}

void test_modup_codegen()
{
	std::string modup = generate_hpu_modup_asm(
		kBconvCfg.num_q,
		kBconvCfg.num_p,
		true);
	std::ofstream("output/modup.cpp") << modup;
	std::cout << "Saved modup ASM to output/modup.cpp\n";
}

void test_moddown_codegen()
{
	std::string moddown = generate_hpu_moddown_asm(
		kModdownCfg.num_q,
		kModdownCfg.num_p,
		true);
	std::ofstream("output/moddown.cpp") << moddown;
	std::cout << "Saved moddown ASM to output/moddown.cpp\n";
}

void test_keyswitch_codegen()
{
	// test with N=4096, num_q=4, num_p=3, dnum=2
	std::string keyswitch = generate_hpu_keyswitch_asm(
		4096, 4, 3, 2, true);
	std::ofstream("output/keyswitch.cpp") << keyswitch;
	std::cout << "Saved keyswitch ASM to output/keyswitch.cpp\n";
}

} // namespace

int main()
{
	std::filesystem::create_directory("output");
	test_ntt_codegen();
	test_intt_codegen();
	test_mm_codegen();
	test_bconv_codegen();
	test_pmult_codegen();
	test_cmult_codegen();
	test_modup_codegen();
	test_moddown_codegen();
	test_keyswitch_codegen();
	return 0;
}
