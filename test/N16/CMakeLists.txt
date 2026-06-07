add_executable(inline_asm_test_n16
        main.cpp
        ../../src/util/mm.cpp
        ../../src/util/ntt.cpp
)

target_link_libraries(inline_asm_test_n16
        PRIVATE
        hpu_encode
)

target_include_directories(inline_asm_test_n16
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(inline_asm_test_n16 PRIVATE cxx_std_17)
