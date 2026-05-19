#pragma once

#include <cstdint>

#include "instruction.hpp"

namespace hpu {

std::uint32_t encode_instruction(const Instruction& instruction);

}  // namespace hpu
