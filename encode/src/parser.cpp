#include "parser.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace hpu {
namespace {

std::string trim(std::string value) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string strip_inline_comments(std::string line) {
    auto erase_from = [&](std::size_t pos) {
        if (pos != std::string::npos) {
            line.erase(pos);
        }
    };

    erase_from(line.find("//"));
    erase_from(line.find('#'));
    erase_from(line.find(';'));

    const auto block_begin = line.find("/*");
    if (block_begin != std::string::npos) {
        const auto block_end = line.find("*/", block_begin + 2);
        if (block_end != std::string::npos) {
            line.erase(block_begin, block_end - block_begin + 2);
        } else {
            line.erase(block_begin);
        }
    }

    return line;
}

std::string unescape_quoted(std::string_view quoted) {
    std::string out;
    out.reserve(quoted.size());
    bool escaping = false;
    for (char ch : quoted) {
        if (escaping) {
            switch (ch) {
                case 'n':
                case 'r':
                case 't':
                    break;
                case '\\':
                case '"':
                    out.push_back(ch);
                    break;
                default:
                    out.push_back(ch);
                    break;
            }
            escaping = false;
            continue;
        }

        if (ch == '\\') {
            escaping = true;
            continue;
        }

        out.push_back(ch);
    }
    return out;
}

bool is_generated_cpp_function_boundary(const std::string& line) {
    return line.rfind("void ", 0) == 0 &&
           line.find("(void)") != std::string::npos &&
           !line.empty() &&
           line.back() == '{';
}

bool is_inline_asm_boundary_line(const std::string& line) {
    return line == "__asm__ volatile(" || line == "asm volatile(" ||
           line == ");" || line == "(" || line == ")" ||
           line == "{" || line == "}" || line == ":" ||
           line == "," || line == ": \"memory\"" ||
           is_generated_cpp_function_boundary(line);
}

std::string normalize_line(std::string_view raw_line) {
    std::string line = trim(strip_inline_comments(std::string(raw_line)));
    if (line.empty()) {
        return {};
    }

    if (is_inline_asm_boundary_line(line)) {
        return {};
    }

    const auto first_quote = line.find('"');
    const auto last_quote = line.rfind('"');
    if (first_quote != std::string::npos && last_quote != first_quote) {
        line = trim(unescape_quoted(line.substr(first_quote + 1, last_quote - first_quote - 1)));
    }

    line = trim(line);
    if (is_inline_asm_boundary_line(line)) {
        return {};
    }

    return line;
}

std::vector<std::string> split_operands(const std::string& operand_text) {
    std::vector<std::string> operands;
    std::string current;
    std::stringstream ss(operand_text);
    while (std::getline(ss, current, ',')) {
        current = trim(current);
        if (!current.empty()) {
            operands.push_back(current);
        }
    }
    return operands;
}

int parse_base0_int(const std::string& token, const std::string& field_name) {
    std::size_t consumed = 0;
    int value = 0;
    try {
        value = std::stoi(token, &consumed, 0);
    } catch (const std::exception&) {
        throw std::runtime_error("invalid integer for " + field_name + ": " + token);
    }

    if (consumed != token.size()) {
        throw std::runtime_error("invalid integer for " + field_name + ": " + token);
    }
    return value;
}

int parse_pobj(const std::string& token, const std::string& field_name) {
    if (token.size() < 2 || token.front() != 'p') {
        throw std::runtime_error("invalid object slot for " + field_name + ": " + token);
    }
    const int value = parse_base0_int(token.substr(1), field_name);
    if (value < 0 || value > 7) {
        throw std::runtime_error("object slot out of range for " + field_name + ": " + token);
    }
    return value;
}

int parse_xreg(const std::string& token, const std::string& field_name) {
    if (token.size() < 2 || token.front() != 'x') {
        throw std::runtime_error("invalid register for " + field_name + ": " + token);
    }
    const int value = parse_base0_int(token.substr(1), field_name);
    if (value < 0 || value > 31) {
        throw std::runtime_error("register out of range for " + field_name + ": " + token);
    }
    return value;
}

void expect_operand_count(const std::vector<std::string>& operands,
                          std::size_t expected,
                          const std::string& mnemonic) {
    if (operands.size() != expected) {
        throw std::runtime_error("unexpected operand count for " + mnemonic);
    }
}

bool is_in_place_transform(Mnemonic mnemonic) {
    return mnemonic == Mnemonic::kPntt || mnemonic == Mnemonic::kPintt;
}

const char* stg_first_operand_name(Mnemonic mnemonic) {
    return is_in_place_transform(mnemonic) ? "pdata" : "pdst";
}

const char* stg_second_operand_name(Mnemonic mnemonic) {
    return is_in_place_transform(mnemonic) ? "ptwiddle" : "psrc";
}

Instruction parse_ar3(Mnemonic mnemonic,
                      const std::vector<std::string>& operands,
                      bool immediate_mode) {
    expect_operand_count(operands, 3, to_string(mnemonic));

    Instruction instruction {};
    instruction.mnemonic = mnemonic;
    instruction.pdst = parse_pobj(operands[0], "pdst");
    instruction.psrc1 = parse_pobj(operands[1], "psrc1");
    if (immediate_mode) {
        instruction.imm8 = parse_base0_int(operands[2], "cimm8");
    } else {
        instruction.psrc2 = parse_pobj(operands[2], "psrc2");
    }
    return instruction;
}

Instruction parse_stg(Mnemonic mnemonic, const std::vector<std::string>& operands) {
    expect_operand_count(operands, 5, to_string(mnemonic));

    Instruction instruction {};
    instruction.mnemonic = mnemonic;
    instruction.pdst = parse_pobj(operands[0], stg_first_operand_name(mnemonic));
    instruction.psrc1 = parse_pobj(operands[1], stg_second_operand_name(mnemonic));
    instruction.idx0 = parse_base0_int(operands[2], "idx0");
    instruction.idx1 = parse_base0_int(operands[3], "idx1");
    instruction.mode = static_cast<std::uint8_t>(parse_base0_int(operands[4], "mode"));
    return instruction;
}

Instruction parse_cfg(Mnemonic mnemonic, const std::vector<std::string>& operands) {
    Instruction instruction {};
    instruction.mnemonic = mnemonic;

    if (mnemonic == Mnemonic::kPseed) {
        expect_operand_count(operands, 1, "pseed");
        instruction.imm21 = static_cast<std::uint32_t>(parse_base0_int(operands[0], "imm21"));
        return instruction;
    }

    expect_operand_count(operands, 3, to_string(mnemonic));
    instruction.idx0 = parse_pobj(operands[0], "idx0");
    instruction.idx1 = parse_base0_int(operands[1], "idx1");
    instruction.cfg = static_cast<std::uint16_t>(parse_base0_int(operands[2], "cfg"));
    return instruction;
}

Instruction parse_sync(const std::vector<std::string>& operands) {
    expect_operand_count(operands, 2, "psync");

    Instruction instruction {};
    instruction.mnemonic = Mnemonic::kPsync;
    instruction.tag = static_cast<std::uint8_t>(parse_base0_int(operands[0], "tag"));
    instruction.mode = static_cast<std::uint8_t>(parse_base0_int(operands[1], "mode"));
    return instruction;
}

Instruction parse_dma(Mnemonic mnemonic, const std::vector<std::string>& operands) {
    expect_operand_count(operands, 4, to_string(mnemonic));

    Instruction instruction {};
    instruction.mnemonic = mnemonic;
    instruction.rs1 = parse_xreg(operands[0], "rs1");
    instruction.rs2 = parse_xreg(operands[1], "rs2");
    instruction.obj_id = static_cast<std::uint8_t>(parse_pobj(operands[2], "obj_id"));
    instruction.type = static_cast<std::uint8_t>(parse_base0_int(operands[3], mnemonic == Mnemonic::kDload ? "load_type" : "rel"));
    return instruction;
}

}  // namespace

Instruction parse_instruction_line(std::string_view line) {
    const std::string normalized = normalize_line(line);
    if (normalized.empty()) {
        throw std::runtime_error("empty instruction line");
    }

    const auto split = normalized.find_first_of(" \t");
    const std::string mnemonic = to_lower(normalized.substr(0, split));
    const std::string operand_text = split == std::string::npos ? std::string() : normalized.substr(split + 1);
    const std::vector<std::string> operands = split_operands(operand_text);

    if (mnemonic == "padd") return parse_ar3(Mnemonic::kPadd, operands, false);
    if (mnemonic == "paddi") return parse_ar3(Mnemonic::kPaddi, operands, true);
    if (mnemonic == "psub") return parse_ar3(Mnemonic::kPsub, operands, false);
    if (mnemonic == "psubi") return parse_ar3(Mnemonic::kPsubi, operands, true);
    if (mnemonic == "pmul") return parse_ar3(Mnemonic::kPmul, operands, false);
    if (mnemonic == "pmuli") return parse_ar3(Mnemonic::kPmuli, operands, true);
    if (mnemonic == "pmac") return parse_ar3(Mnemonic::kPmac, operands, false);
    if (mnemonic == "pmaci") return parse_ar3(Mnemonic::kPmaci, operands, true);

    if (mnemonic == "pntt") return parse_stg(Mnemonic::kPntt, operands);
    if (mnemonic == "pintt") return parse_stg(Mnemonic::kPintt, operands);
    if (mnemonic == "pshuf") return parse_stg(Mnemonic::kPshuf, operands);
    if (mnemonic == "psample") return parse_stg(Mnemonic::kPsample, operands);

    if (mnemonic == "pshcfg") return parse_cfg(Mnemonic::kPshcfg, operands);
    if (mnemonic == "pseed") return parse_cfg(Mnemonic::kPseed, operands);
    if (mnemonic == "pmodld") return parse_cfg(Mnemonic::kPmodld, operands);

    if (mnemonic == "psync") return parse_sync(operands);

    if (mnemonic == "dload") return parse_dma(Mnemonic::kDload, operands);
    if (mnemonic == "dstore") return parse_dma(Mnemonic::kDstore, operands);

    throw std::runtime_error("unsupported mnemonic: " + mnemonic);
}

std::vector<Instruction> parse_source(std::string_view source) {
    std::vector<Instruction> instructions;
    std::stringstream ss{std::string(source)};
    std::string line;
    while (std::getline(ss, line)) {
        const std::string normalized = normalize_line(line);
        if (normalized.empty()) {
            continue;
        }
        instructions.push_back(parse_instruction_line(normalized));
    }
    return instructions;
}

}  // namespace hpu
