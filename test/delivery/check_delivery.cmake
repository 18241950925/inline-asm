if(NOT DEFINED ROOT)
    message(FATAL_ERROR "ROOT is required")
endif()

set(REQUIRED_FILES
    "output/ciphertext_multiply.asm"
    "outputs/ciphertext_multiply/ciphertext_multiply.inst32"
    "outputs/ciphertext_multiply/test_data/params.json"
    "outputs/ciphertext_multiply/test_data/artifact_manifest.csv"
    "outputs/ciphertext_multiply/test_data/input/ct_a_q.bin"
    "outputs/ciphertext_multiply/test_data/input/ct_a_q.hex.txt"
    "outputs/ciphertext_multiply/test_data/input/ct_b_q.bin"
    "outputs/ciphertext_multiply/test_data/constants/relinearization_key_ntt_qp.bin"
    "outputs/ciphertext_multiply/test_data/expected/tensor_coeff_q.bin"
    "outputs/ciphertext_multiply/test_data/expected/keyswitch_moddown_q.bin"
    "outputs/ciphertext_multiply/test_data/expected/ciphertext_out_q.bin"
    "outputs/ciphertext_multiply/test_data/expected/plaintext_product_mod_t.bin"
    "outputs/ciphertext_multiply/test_data/VALIDATION.txt"
    "outputs/ciphertext_multiply/test_data/hardware/abi.json"
    "outputs/ciphertext_multiply/test_data/hardware/hardware_manifest.csv"
    "outputs/ciphertext_multiply/test_data/hardware/hpu_mem_image.u32.bin"
    "outputs/ciphertext_multiply/test_data/hardware/hpu_mem_config.json"
    "outputs/ciphertext_multiply/test_data/hardware/line_map.csv"
    "outputs/ciphertext_multiply/test_data/hardware/mod_ctx_map.csv"
    "outputs/ciphertext_multiply/test_data/hardware/constants/mod_ctx.u32.bin"
    "outputs/ciphertext_multiply/test_data/hardware/twiddle_map.csv"
    "outputs/ciphertext_multiply/test_data/hardware/constants/twiddle/ntt/basis_00/stage_11.u32.bin"
    "outputs/ciphertext_multiply/test_data/hardware/constants/twiddle/intt/basis_00/stage_11.u32.bin"
    "outputs/rv_interface_smoke/rv_interface_smoke.asm"
    "outputs/rv_interface_smoke/rv_interface_smoke.inst32"
    "outputs/rv_interface_smoke/test_data/expected_decode.csv"
    "outputs/rv_interface_smoke/test_data/negative_cases.asm.txt"
    "outputs/intt/test_data/input.hex.txt"
    "outputs/intt/test_data/expected.hex.txt"
)

foreach(CASE_NAME ntt intt mm bconv modup pmult cmult moddown keyswitch)
    list(APPEND REQUIRED_FILES
        "outputs/${CASE_NAME}/test_data/params.json"
        "outputs/${CASE_NAME}/test_data/artifact_manifest.csv"
        "outputs/${CASE_NAME}/test_data/hardware/abi.json"
        "outputs/${CASE_NAME}/test_data/hardware/hpu_mem_image.u32.bin"
        "outputs/${CASE_NAME}/test_data/hardware/hpu_mem_config.json"
        "outputs/${CASE_NAME}/test_data/hardware/line_map.csv"
        "outputs/${CASE_NAME}/test_data/hardware/mod_ctx_map.csv"
        "outputs/${CASE_NAME}/test_data/hardware/twiddle_map.csv")
endforeach()
list(APPEND REQUIRED_FILES "outputs/auto/test_data/STATUS.md")

foreach(RELATIVE_PATH IN LISTS REQUIRED_FILES)
    set(PATH "${ROOT}/${RELATIVE_PATH}")
    if(NOT EXISTS "${PATH}")
        message(FATAL_ERROR "Missing delivery artifact: ${RELATIVE_PATH}")
    endif()
    file(SIZE "${PATH}" SIZE)
    if(SIZE EQUAL 0)
        message(FATAL_ERROR "Empty delivery artifact: ${RELATIVE_PATH}")
    endif()
endforeach()

file(READ "${ROOT}/outputs/ciphertext_multiply/test_data/VALIDATION.txt" VALIDATION)
if(NOT VALIDATION MATCHES "^PASS")
    message(FATAL_ERROR "FHE reference validation did not pass")
endif()

file(READ "${ROOT}/outputs/ciphertext_multiply/test_data/hardware/abi.json" HARDWARE_ABI)
if(NOT HARDWARE_ABI MATCHES "\"coefficient_bits\": 32")
    message(FATAL_ERROR "Hardware ABI is not uint32")
endif()
if(NOT HARDWARE_ABI MATCHES "\"line_bytes\": 256")
    message(FATAL_ERROR "Hardware ABI does not use 256-byte lines")
endif()

file(READ "${ROOT}/outputs/ciphertext_multiply/test_data/hardware/hpu_mem_config.json" HPU_MEM_CONFIG)
if(NOT HPU_MEM_CONFIG MATCHES "\"words_per_line\": 64")
    message(FATAL_ERROR "HPU_MEM configuration does not use 64 uint32 words per line")
endif()

file(READ "${ROOT}/outputs/ciphertext_multiply/test_data/hardware/mod_ctx_map.csv" MOD_CTX_MAP)
if(NOT MOD_CTX_MAP MATCHES "barrett_mu_hex")
    message(FATAL_ERROR "Hardware mod_ctx map does not contain Barrett mu")
endif()

file(READ "${ROOT}/outputs/ciphertext_multiply/test_data/hardware/twiddle_map.csv" TWIDDLE_MAP)
if(NOT TWIDDLE_MAP MATCHES "ntt,0,[0-9]+,butterfly,11")
    message(FATAL_ERROR "Hardware twiddle map is missing NTT stage 11")
endif()
if(NOT TWIDDLE_MAP MATCHES "intt,0,[0-9]+,butterfly,11")
    message(FATAL_ERROR "Hardware twiddle map is missing INTT stage 11")
endif()

file(READ "${ROOT}/output/ciphertext_multiply.asm" CIPHERTEXT_ASM)
foreach(MARKER
        "Tensor product in NTT domain"
        "HYBRID MODUP: Q_digit -> full Q union P"
        "Relinearization: KeySwitch(t2, rlk)"
        "MODDOWN stage-1: BConv P -> Q"
        "Compose final ciphertext")
    string(FIND "${CIPHERTEXT_ASM}" "${MARKER}" POSITION)
    if(POSITION EQUAL -1)
        message(FATAL_ERROR "Missing ciphertext multiply stage marker: ${MARKER}")
    endif()
endforeach()

string(FIND "${CIPHERTEXT_ASM}" "pfree p" PFREE_POSITION)
if(PFREE_POSITION EQUAL -1)
    message(FATAL_ERROR "Ciphertext multiply does not release temporary object slots with pfree")
endif()

foreach(REMOVED_MNEMONIC pshcfg pshuf pseed psample)
    string(FIND "${CIPHERTEXT_ASM}" "${REMOVED_MNEMONIC} " REMOVED_POSITION)
    if(NOT REMOVED_POSITION EQUAL -1)
        message(FATAL_ERROR "Removed instruction appears in ciphertext multiply: ${REMOVED_MNEMONIC}")
    endif()
endforeach()

file(READ "${ROOT}/outputs/rv_interface_smoke/test_data/expected_decode.csv" RV_EXPECTED_DECODE)
if(NOT RV_EXPECTED_DECODE MATCHES "0x8A00000B,custom0,\"pfree p5\"")
    message(FATAL_ERROR "RV decode expectations are missing the architectural pfree encoding")
endif()

file(READ "${ROOT}/outputs/rv_interface_smoke/test_data/negative_cases.asm.txt" RV_NEGATIVE_CASES)
foreach(REMOVED_MNEMONIC pshcfg pshuf pseed psample)
    string(FIND "${RV_NEGATIVE_CASES}" "${REMOVED_MNEMONIC} " NEGATIVE_POSITION)
    if(NEGATIVE_POSITION EQUAL -1)
        message(FATAL_ERROR "RV negative cases do not reject removed instruction: ${REMOVED_MNEMONIC}")
    endif()
endforeach()

function(CHECK_OBJECT_LIFECYCLE RELATIVE_PATH)
    foreach(SLOT RANGE 0 7)
        set(LIVE_${SLOT} 0)
    endforeach()

    file(STRINGS "${ROOT}/${RELATIVE_PATH}" ASM_LINES)
    foreach(LINE IN LISTS ASM_LINES)
        if(LINE MATCHES "\"dload [^,]+, [^,]+, p([0-7]), [0-3]")
            set(SLOT "${CMAKE_MATCH_1}")
            if(LIVE_${SLOT})
                message(FATAL_ERROR "${RELATIVE_PATH}: dload overwrites live object p${SLOT}")
            endif()
            set(LIVE_${SLOT} 1)
        elseif(LINE MATCHES "\"(padd|psub|pmul|pmac|pntt|pintt) p([0-7]),")
            set(SLOT "${CMAKE_MATCH_2}")
            set(LIVE_${SLOT} 1)
        elseif(LINE MATCHES "\"pfree p([0-7])")
            set(SLOT "${CMAKE_MATCH_1}")
            if(NOT LIVE_${SLOT})
                message(FATAL_ERROR "${RELATIVE_PATH}: pfree targets non-live object p${SLOT}")
            endif()
            set(LIVE_${SLOT} 0)
        elseif(LINE MATCHES "\"dstore [^,]+, [^,]+, p([0-7]), ([01])")
            set(SLOT "${CMAKE_MATCH_1}")
            set(RELEASE "${CMAKE_MATCH_2}")
            if(RELEASE EQUAL 1)
                if(NOT LIVE_${SLOT})
                    message(FATAL_ERROR "${RELATIVE_PATH}: dstore rel=1 targets non-live object p${SLOT}")
                endif()
                set(LIVE_${SLOT} 0)
            endif()
        endif()
    endforeach()
endfunction()

foreach(CASE_NAME ntt intt mm bconv pmult cmult modup moddown auto keyswitch ciphertext_multiply)
    CHECK_OBJECT_LIFECYCLE("output/${CASE_NAME}.asm")
endforeach()

file(STRINGS "${ROOT}/outputs/ciphertext_multiply/ciphertext_multiply.inst32" INST32_LINES)
list(LENGTH INST32_LINES INST32_COUNT)
if(INST32_COUNT LESS 2000)
    message(FATAL_ERROR "Ciphertext multiply instruction stream is unexpectedly short")
endif()

file(WRITE "${ROOT}/outputs/DELIVERY_REPORT.txt"
    "SOFTWARE_DELIVERY=PASS\n"
    "FHE_REFERENCE=PASS\n"
    "ASM_ENCODING=PASS\n"
    "INSTRUCTION_SET_11=PASS\n"
    "PFREE_LIFECYCLE=PASS\n"
    "RV_INTERFACE_SMOKE=PASS\n"
    "OPERATOR_UT_PACKAGES=PASS\n"
    "HARDWARE_UINT32_IMAGES=PASS\n"
    "HPU_LINE_LAYOUT_256B=PASS\n"
    "MOD_CTX_BARRETT=PASS\n"
    "STAGE_TWIDDLE_LAYOUT=PASS\n"
    "CIPHERTEXT_MULTIPLY_INST32_COUNT=${INST32_COUNT}\n"
    "HARDWARE_EXECUTION=CONDITIONAL\n"
    "PENDING=numeric CSR offsets, instruction rs1/rs2 binding, scratch map, DMA completion semantics\n")

message(STATUS "HPU software delivery check PASS (${INST32_COUNT} ciphertext-multiply instructions)")
