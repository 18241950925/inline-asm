#pragma once

#include <string>

std::string generate_hpu_modup_body_asm(
    int num_q_digit,
    int num_p,
    int q_offset = 0,
    bool append_psync = false);

// Hybrid key-switch ModUp. The source digit is Q[q_offset:q_offset +
// num_q_digit); the output is materialized in the full Q union P basis.
std::string generate_hpu_hybrid_modup_body_asm(
    int num_q,
    int num_p,
    int num_q_digit,
    int q_offset,
    bool append_psync = false);

std::string generate_hpu_modup_asm(
    int num_q_digit,
    int num_p,
    int q_offset = 0,
    bool append_psync = false);
