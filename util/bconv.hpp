#pragma once

#include <stdint.h>
#include <string>

std::string generate_hpu_bconv_asm(
  int N,
  int l,
  uint16_t base_addr_in,
  uint16_t base_addr_acc,
  int mod_in,
  int mod_out,
  int cst_base_id);
