#pragma once

#include <string>

std::string generate_hpu_modup_body_asm(
    int num_q_digit,
    int num_p,
    int q_offset = 0,
    bool append_psync = false);

std::string generate_hpu_modup_asm(
    int num_q_digit,
    int num_p,
    int q_offset = 0,
    bool append_psync = false);
