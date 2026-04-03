#include <iostream>
#include <string>

#include "util/bconv.hpp"
#include "util/mm.hpp"
#include "util/ntt.hpp"

namespace {

struct NttConfig {
	int N;
	int l;
	uint16_t base_addr_in;
	uint16_t base_addr_out;
	int mod_id;
};

struct MmConfig {
	int N;
	int l;
	uint16_t base_addr_a;
	uint16_t base_addr_b;
	uint16_t base_addr_c;
	int mod_id;
};

struct BconvConfig {
	int N;
	int l;
	int num_q;
	int num_p;
	uint16_t base_addr_in;
	uint16_t base_addr_tmp;
	uint16_t base_addr_out;
};

constexpr NttConfig kNttCfg{64, 16, 0x0000, 0x0080, 0};
constexpr MmConfig kMmCfg{64, 16, 0x0100, 0x0200, 0x0300, 1};
constexpr BconvConfig kBconvCfg{64, 16, 2, 3, 0x0400, 0x0500, 0x0600};

void test_ntt_codegen()
{
	std::string ntt = generate_hpu_ntt_asm(
		kNttCfg.N,
		kNttCfg.l,
		kNttCfg.base_addr_in,
		kNttCfg.base_addr_out,
		kNttCfg.mod_id);
	std::cout << "===== NTT ASM =====\n" << ntt << "\n";
}

void test_mm_codegen()
{
	std::string mm = generate_hpu_mm_asm(
		kMmCfg.N,
		kMmCfg.l,
		kMmCfg.base_addr_a,
		kMmCfg.base_addr_b,
		kMmCfg.base_addr_c,
		kMmCfg.mod_id);
	std::cout << "===== MM ASM =====\n" << mm << "\n";
}

void test_bconv_codegen()
{
	std::string bconv = generate_hpu_bconv_asm(
		kBconvCfg.N,
		kBconvCfg.l,
		kBconvCfg.num_q,
		kBconvCfg.num_p,
		kBconvCfg.base_addr_in,
		kBconvCfg.base_addr_tmp,
		kBconvCfg.base_addr_out);
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
