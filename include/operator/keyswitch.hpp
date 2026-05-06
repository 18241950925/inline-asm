#pragma once

#include <string>

std::string generate_hpu_keyswitch_body_asm(
    int N,
    int num_q,
    int num_p,
    int dnum,
    bool append_psync = false);

std::string generate_hpu_keyswitch_asm(
    int N,
    int num_q,
    int num_p,
    int dnum,
    bool append_psync = false);
