add_executable(inline_asm_encode_outputs
        main.cpp
)

target_link_libraries(inline_asm_encode_outputs
        PRIVATE
        hpu_encode
)

target_compile_features(inline_asm_encode_outputs PRIVATE cxx_std_17)
