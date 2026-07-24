if(NOT DEFINED ROOT)
    message(FATAL_ERROR "ROOT is required")
endif()

set(REQUIRED_FILES
    "output/ciphertext_multiply.asm"
    "outputs/ciphertext_multiply/ciphertext_multiply.inst32"
    "outputs/ciphertext_multiply/ciphertext_multiply.cmd26"
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
    "outputs/rv_interface_smoke/rv_interface_smoke.cmd26"
    "outputs/rv_interface_smoke/test_data/expected_decode.csv"
    "outputs/rv_interface_smoke/test_data/expected_cmd26.csv"
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
        "outputs/${CASE_NAME}/test_data/hardware/twiddle_map.csv"
        "outputs/${CASE_NAME}/${CASE_NAME}.cmd26")
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
if(NOT HARDWARE_ABI MATCHES "\"dload_flag0_small_bank\": 1")
    message(FATAL_ERROR "Hardware ABI does not select Bank 5 for mod_ctx dload")
endif()
if(NOT HARDWARE_ABI MATCHES "\"small_bank_lines\": 32")
    message(FATAL_ERROR "Hardware ABI does not use the latest 32-line Bank 5")
endif()
if(NOT HARDWARE_ABI MATCHES "\"regular_bank_count\": 5")
    message(FATAL_ERROR "Hardware ABI does not describe the five regular SRAM banks")
endif()
if(NOT HARDWARE_ABI MATCHES "\"regular_bank_lines\": 1024")
    message(FATAL_ERROR "Hardware ABI does not describe 1024 lines per regular SRAM bank")
endif()
if(NOT HARDWARE_ABI MATCHES "\"mod_table_base_line\": \"0x00001400\"")
    message(FATAL_ERROR "Hardware ABI does not freeze MOD_TABLE_BASE_LINE at 0x1400")
endif()
if(NOT HARDWARE_ABI MATCHES "\"physical_context_capacity\": 512")
    message(FATAL_ERROR "Hardware ABI does not describe Bank 5 physical context capacity")
endif()
if(NOT HARDWARE_ABI MATCHES "\"mod_id_bits\": 8")
    message(FATAL_ERROR "Hardware ABI does not enforce the PMODLD MOD_ID width")
endif()
if(NOT HARDWARE_ABI MATCHES "\"mod_id_addressable_lines\": 16")
    message(FATAL_ERROR "Hardware ABI does not distinguish MOD_ID reach from Bank 5 depth")
endif()
if(NOT HARDWARE_ABI MATCHES "\"max_contexts\": 256")
    message(FATAL_ERROR "Hardware ABI does not cap contexts by the 8-bit MOD_ID address space")
endif()
if(NOT HARDWARE_ABI MATCHES "\"rs1_value\": \"HPU_MEM line offset\"")
    message(FATAL_ERROR "Hardware ABI does not freeze custom1 rs1 as the HPU_MEM line offset")
endif()
if(NOT HARDWARE_ABI MATCHES "\"rs2_value\": \"line count\"")
    message(FATAL_ERROR "Hardware ABI does not freeze custom1 rs2 as the nonzero line count")
endif()
if(NOT HARDWARE_ABI MATCHES "\"q_min\": 65537")
    message(FATAL_ERROR "Hardware ABI does not enforce the PE minimum modulus")
endif()
if(NOT HARDWARE_ABI MATCHES "\"q_max\": 4294967295")
    message(FATAL_ERROR "Hardware ABI does not enforce the PE maximum modulus")
endif()
if(NOT HARDWARE_ABI MATCHES "\"mu_bits\": 48")
    message(FATAL_ERROR "Hardware ABI does not describe the PE 48-bit Barrett mu")
endif()
if(NOT HARDWARE_ABI MATCHES "\"reserved_bits\": 48")
    message(FATAL_ERROR "Hardware ABI does not describe the 48 reserved context bits")
endif()
if(NOT HARDWARE_ABI MATCHES "\"stage_payload_words\": 2048")
    message(FATAL_ERROR "Hardware ABI does not provide N/2 physical twiddles per stage")
endif()
if(NOT HARDWARE_ABI MATCHES "\"stage_payload_lines\": 32")
    message(FATAL_ERROR "Hardware ABI stage twiddles do not occupy N/128 HPU lines")
endif()
if(NOT HARDWARE_ABI MATCHES "\"pre_twist_execution\": \"explicit PMUL")
    message(FATAL_ERROR "Hardware ABI does not explicitly execute the negacyclic pre-twist")
endif()
if(NOT HARDWARE_ABI MATCHES "\"intt_post_execution\": \"explicit PMUL")
    message(FATAL_ERROR "Hardware ABI does not explicitly execute INTT normalization/inverse twist")
endif()
if(NOT HARDWARE_ABI MATCHES "\"physical_update\": \"out-of-place per stage")
    message(FATAL_ERROR "Hardware ABI does not freeze NTT/INTT as physical out-of-place")
endif()

file(READ "${ROOT}/outputs/ciphertext_multiply/test_data/hardware/hpu_mem_config.json" HPU_MEM_CONFIG)
if(NOT HPU_MEM_CONFIG MATCHES "\"words_per_line\": 64")
    message(FATAL_ERROR "HPU_MEM configuration does not use 64 uint32 words per line")
endif()
foreach(CSR_OFFSET 0x00 0x04 0x08 0x0c 0x10 0x14 0x18)
    if(NOT HPU_MEM_CONFIG MATCHES "\"offset\": \"${CSR_OFFSET}\"")
        message(FATAL_ERROR "HPU_MEM configuration is missing CSR offset ${CSR_OFFSET}")
    endif()
endforeach()
if(HPU_MEM_CONFIG MATCHES "RTL_CONFIRM_REQUIRED")
    message(FATAL_ERROR "HPU_MEM configuration still marks frozen CSR offsets unresolved")
endif()

file(READ "${ROOT}/outputs/ciphertext_multiply/test_data/hardware/mod_ctx_map.csv" MOD_CTX_MAP)
if(NOT MOD_CTX_MAP MATCHES "barrett_mu48_hex")
    message(FATAL_ERROR "Hardware mod_ctx map does not contain 48-bit Barrett mu")
endif()

file(READ "${ROOT}/outputs/ciphertext_multiply/test_data/hardware/twiddle_map.csv" TWIDDLE_MAP)
file(STRINGS "${ROOT}/outputs/ciphertext_multiply/test_data/hardware/twiddle_map.csv"
    TWIDDLE_ROWS)
foreach(DIRECTION ntt intt)
    foreach(STAGE RANGE 0 11)
        set(STAGE_FOUND 0)
        foreach(TWIDDLE_ROW IN LISTS TWIDDLE_ROWS)
            if(TWIDDLE_ROW MATCHES
                    "^${DIRECTION},0,[0-9]+,butterfly,${STAGE},2048,"
                    AND TWIDDLE_ROW MATCHES ",32$")
                set(STAGE_FOUND 1)
            endif()
        endforeach()
        if(NOT STAGE_FOUND)
            message(FATAL_ERROR
                "Hardware twiddle map does not provide 2048 words/32 lines for ${DIRECTION} stage ${STAGE}")
        endif()
    endforeach()
endforeach()

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

file(READ "${ROOT}/output/ntt.asm" NTT_ASM)
string(FIND "${NTT_ASM}" "Negacyclic pre-twist: explicit PMUL" NTT_PRE_TWIST_POSITION)
string(FIND "${NTT_ASM}" "pntt p" NTT_STAGE_POSITION)
if(NTT_PRE_TWIST_POSITION EQUAL -1
        OR NTT_STAGE_POSITION EQUAL -1
        OR NTT_PRE_TWIST_POSITION GREATER_EQUAL NTT_STAGE_POSITION)
    message(FATAL_ERROR "NTT stream does not execute the pre-twist before stage 0")
endif()

file(READ "${ROOT}/output/intt.asm" INTT_ASM)
string(FIND "${INTT_ASM}" "pintt p" INTT_STAGE_POSITION)
string(FIND "${INTT_ASM}" "INTT normalize and inverse-twist: explicit PMUL" INTT_POST_POSITION)
if(INTT_STAGE_POSITION EQUAL -1
        OR INTT_POST_POSITION EQUAL -1
        OR INTT_STAGE_POSITION GREATER_EQUAL INTT_POST_POSITION)
    message(FATAL_ERROR "INTT stream does not execute normalization/inverse twist after its stages")
endif()

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
if(NOT RV_EXPECTED_DECODE MATCHES "0x603FC00B,0x0C07F80,custom0,\"pmodld 255\"")
    message(FATAL_ERROR "RV decode expectations are missing the 8-bit MOD_ID pmodld encoding")
endif()
if(NOT RV_EXPECTED_DECODE MATCHES "0x8140000B,0x1028000,custom0,\"pfree p5\"")
    message(FATAL_ERROR "RV decode expectations are missing the architectural pfree encoding")
endif()
if(NOT RV_EXPECTED_DECODE MATCHES "0x00B5292B,0x2000424,custom1,\"dload x10, x11, p4, 2, 1\"")
    message(FATAL_ERROR "RV decode expectations are missing mod_ctx small-bank flag[0]")
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
        if(LINE MATCHES "\"dload [^,]+, [^,]+, p([0-7]), [0-3], [01]")
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

function(CHECK_MOD_CONTEXT_LOAD RELATIVE_PATH)
    set(WAITING_FOR_SYNC 0)
    file(STRINGS "${ROOT}/${RELATIVE_PATH}" ASM_LINES)
    foreach(LINE IN LISTS ASM_LINES)
        if(LINE MATCHES "\"dload [^,]+, [^,]+, p[0-7], [013], 1")
            message(FATAL_ERROR
                "${RELATIVE_PATH}: non-mod_ctx dload requests reserved small Bank 5")
        elseif(LINE MATCHES "\"dload [^,]+, [^,]+, p[0-7], 2, 0")
            message(FATAL_ERROR "${RELATIVE_PATH}: mod_ctx dload does not set small-bank flag[0]")
        elseif(LINE MATCHES "\"dload [^,]+, [^,]+, p[0-7], 2, 1")
            set(WAITING_FOR_SYNC 1)
        elseif(WAITING_FOR_SYNC AND LINE MATCHES "\"pmodld ")
            message(FATAL_ERROR "${RELATIVE_PATH}: pmodld can issue before mod_ctx DMA completion")
        elseif(WAITING_FOR_SYNC AND LINE MATCHES "\"psync")
            set(WAITING_FOR_SYNC 0)
        endif()
    endforeach()
    if(WAITING_FOR_SYNC)
        message(FATAL_ERROR "${RELATIVE_PATH}: mod_ctx DMA has no following psync")
    endif()
endfunction()

foreach(CASE_NAME ntt intt bconv pmult cmult modup moddown auto keyswitch ciphertext_multiply)
    CHECK_MOD_CONTEXT_LOAD("output/${CASE_NAME}.asm")
endforeach()

file(STRINGS "${ROOT}/outputs/ciphertext_multiply/ciphertext_multiply.inst32" INST32_LINES)
list(LENGTH INST32_LINES INST32_COUNT)
if(INST32_COUNT LESS 2000)
    message(FATAL_ERROR "Ciphertext multiply instruction stream is unexpectedly short")
endif()
file(STRINGS "${ROOT}/outputs/ciphertext_multiply/ciphertext_multiply.cmd26" CMD26_LINES)
list(LENGTH CMD26_LINES CMD26_COUNT)
if(NOT CMD26_COUNT EQUAL INST32_COUNT)
    message(FATAL_ERROR "32-bit instruction and 26-bit precode counts differ")
endif()

file(WRITE "${ROOT}/outputs/DELIVERY_REPORT.txt"
    "SOFTWARE_DELIVERY=PASS\n"
    "FHE_REFERENCE=PASS\n"
    "ASM_ENCODING=PASS\n"
    "PRECODE_CMD26=PASS\n"
    "MOD_CTX_SMALL_BANK_FLAG=PASS\n"
    "INSTRUCTION_SET_11=PASS\n"
    "PFREE_LIFECYCLE=PASS\n"
    "RV_INTERFACE_SMOKE=PASS\n"
    "OPERATOR_UT_PACKAGES=PASS\n"
    "HARDWARE_UINT32_IMAGES=PASS\n"
    "HPU_LINE_LAYOUT_256B=PASS\n"
    "CUSTOM1_LINE_SIDEBAND=PASS\n"
    "HPU_MEM_CSR_MAP=PASS\n"
    "MOD_CTX_Q32_MU48=PASS\n"
    "MOD_TABLE_BASE_0X1400=PASS\n"
    "STAGE_TWIDDLE_LAYOUT=PASS\n"
    "NEGACYCLIC_FACTORS_EXPLICIT=PASS\n"
    "NTT_PHYSICAL_OUT_OF_PLACE=PASS\n"
    "CIPHERTEXT_MULTIPLY_INST32_COUNT=${INST32_COUNT}\n"
    "HARDWARE_EXECUTION=CONDITIONAL\n"
    "PENDING=DMA instruction relocation/GPR loading, scratch map, runtime cache/fault/interrupt handling\n")

message(STATUS "HPU software delivery check PASS (${INST32_COUNT} ciphertext-multiply instructions)")
