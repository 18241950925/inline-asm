#pragma once

#include <stdint.h>
#include <string>

std::string generate_hpu_bconv_asm(
    int N, int l, 
    int num_q, int num_p,
    uint16_t base_addr_in, 
    uint16_t base_addr_tmp, 
    uint16_t base_addr_out) ;
