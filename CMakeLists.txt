cmake_minimum_required(VERSION 3.16)

project(inline_asm_codegen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(inline_asm_codegen
        src/main.cpp
        src/util/ntt.cpp
        src/util/mm.cpp
        src/util/bconv.cpp
        src/operator/modup.cpp
        src/operator/moddown.cpp
        src/operator/pmult.cpp
        src/operator/cmult.cpp
)

target_include_directories(inline_asm_codegen
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
