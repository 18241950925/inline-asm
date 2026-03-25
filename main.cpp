#include <iostream>
#include <string>

#include "util/bconv.hpp"
#include "util/mm.hpp"
#include "util/ntt.hpp"

namespace {

void test_ntt_codegen()
{
	std::string ntt = generate_hpu_ntt_asm(64, 16, 0x0000, 0);
	std::cout << "===== NTT ASM =====\n" << ntt << "\n";
}

void test_mm_codegen()
{
	std::string mm = generate_hpu_mm_asm(64, 16, 0x0100, 0x0200, 0x0300, 1);
	std::cout << "===== MM ASM =====\n" << mm << "\n";
}

void test_bconv_codegen()
{
	std::string bconv = generate_hpu_bconv_asm(64, 16, 0x0400, 0x0500, 2, 3, 100);
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
