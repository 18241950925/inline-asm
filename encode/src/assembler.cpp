#include "assembler.hpp"

#include <iomanip>
#include <sstream>

#include "encoder.hpp"
#include "parser.hpp"

namespace hpu {

EncodedInstruction assemble_line(std::string_view line) {
    EncodedInstruction encoded;
    encoded.instruction = parse_instruction_line(line);
    encoded.word = encode_instruction(encoded.instruction);
    encoded.normalized_asm = to_string(encoded.instruction);
    return encoded;
}

std::vector<EncodedInstruction> assemble_source(std::string_view source) {
    std::vector<EncodedInstruction> encoded;
    for (const auto& instruction : parse_source(source)) {
        encoded.push_back({instruction, encode_instruction(instruction), to_string(instruction)});
    }
    return encoded;
}

std::string format_word_hex(std::uint32_t word) {
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << word;
    return oss.str();
}

}  // namespace hpu
