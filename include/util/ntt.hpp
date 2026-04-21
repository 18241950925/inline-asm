#pragma once

#include <string>

std::string generate_hpu_ntt_body_asm(
        int N,
        int obj_poly_a,
        int obj_poly_b,
        int mod_ctx_obj,
        int shf_cfg_obj,
        bool append_psync = false);

std::string generate_hpu_intt_body_asm(
        int N,
        int obj_poly_a,
        int obj_poly_b,
        int mod_ctx_obj,
        int shf_cfg_obj,
        bool append_psync = false);

std::string generate_hpu_ntt_asm(
        int N,
        int obj_poly_a,
        int obj_poly_b,
        int mod_ctx_obj,
        int shf_cfg_obj,
        bool append_psync = false);

std::string generate_hpu_intt_asm(
        int N,
        int obj_poly_a,
        int obj_poly_b,
        int mod_ctx_obj,
        int shf_cfg_obj,
        bool append_psync = false);
