#pragma once

#include <stdint.h>
#include <string>

std::string generate_hpu_ntt_asm(int N, int l, uint16_t base_addr, int mod_id);

