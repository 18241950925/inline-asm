cmake_minimum_required(VERSION 3.16)

project(inline_asm_codegen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(inline_asm_codegen
	main.cpp
	util/ntt.cpp
	util/mm.cpp
	util/bconv.cpp
)

target_include_directories(inline_asm_codegen
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
)
