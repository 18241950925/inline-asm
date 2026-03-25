#pragma once

#include <stdint.h>
#include <string>

std::string generate_hpu_mm_asm(
  int N,
  int l,
  uint16_t base_addr_a,
  uint16_t base_addr_b,
  uint16_t base_addr_c,
  int mod_id);

