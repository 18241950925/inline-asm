#include "encoder.hpp"

#include <stdexcept>

namespace hpu {
namespace {

constexpr std::uint32_t kCustom0Opcode = 0b0001011;
constexpr std::uint32_t kCustom1Opcode = 0b0101011;

constexpr std::uint32_t kOpcPadd = 0b0000;
constexpr std::uint32_t kOpcPsub = 0b0001;
constexpr std::uint32_t kOpcPmul = 0b0010;
constexpr std::uint32_t kOpcPmac = 0b0011;
constexpr std::uint32_t kOpcPntt = 0b0100;
constexpr std::uint32_t kOpcPintt = 0b0101;
constexpr std::uint32_t kOpcPmodld = 0b0110;
constexpr std::uint32_t kOpcPsync = 0b0111;
constexpr std::uint32_t kOpcPfree = 0b1000;

void ensure_range(int value, int min_value, int max_value, const char* field_name) {
    if (value < min_value || value > max_value) {
        throw std::runtime_error(std::string(field_name) + " is out of range");
    }
}

std::uint32_t opcode_for(Mnemonic mnemonic) {
    switch (mnemonic) {
        case Mnemonic::kPadd:
            return kOpcPadd;
        case Mnemonic::kPsub:
            return kOpcPsub;
        case Mnemonic::kPmul:
            return kOpcPmul;
        case Mnemonic::kPmac:
            return kOpcPmac;
        case Mnemonic::kPntt:
            return kOpcPntt;
        case Mnemonic::kPintt:
            return kOpcPintt;
        case Mnemonic::kPmodld:
            return kOpcPmodld;
        case Mnemonic::kPfree:
            return kOpcPfree;
        case Mnemonic::kPsync:
            return kOpcPsync;
        default:
            throw std::runtime_error("opcode is undefined for mnemonic");
    }
}

std::uint32_t encode_ar3(const Instruction& instruction) {
    ensure_range(instruction.pdst, 0, 7, "pdst");
    ensure_range(instruction.psrc1, 0, 7, "psrc1");
    ensure_range(instruction.mode, 0, 0xF, "mode");
    ensure_range(instruction.flag, 0, 0x7, "flag");

    std::uint32_t op2 = 0;
    std::uint32_t mode = instruction.mode;
    if (instruction.imm8 >= 0) {
        if (instruction.mnemonic != Mnemonic::kPmul
            && instruction.mnemonic != Mnemonic::kPmac) {
            throw std::runtime_error("only pmul/pmac support cimm8");
        }
        ensure_range(instruction.imm8, 0, 0xFF, "cimm8");
        op2 = static_cast<std::uint32_t>(instruction.imm8);
        mode |= 0b0001;
    } else {
        ensure_range(instruction.psrc2, 0, 7, "psrc2");
        op2 = static_cast<std::uint32_t>(instruction.psrc2);
    }

    std::uint32_t word = 0;
    word |= opcode_for(instruction.mnemonic) << 28;
    word |= static_cast<std::uint32_t>(instruction.pdst) << 25;
    word |= static_cast<std::uint32_t>(instruction.psrc1) << 22;
    word |= op2 << 14;
    word |= mode << 10;
    word |= static_cast<std::uint32_t>(instruction.flag) << 7;
    word |= kCustom0Opcode;
    return word;
}

std::uint32_t encode_stg(const Instruction& instruction) {
    ensure_range(instruction.pdst, 0, 7, "pdata");
    ensure_range(instruction.psrc1, 0, 7, "ptwiddle");
    ensure_range(instruction.idx0, 0, 0xF, "idx0");
    ensure_range(instruction.idx1, 0, 0xF, "idx1");
    ensure_range(instruction.mode, 0, 0xF, "mode");
    ensure_range(instruction.flag, 0, 0x7, "flag");

    std::uint32_t word = 0;
    word |= opcode_for(instruction.mnemonic) << 28;
    word |= static_cast<std::uint32_t>(instruction.pdst) << 25;
    word |= static_cast<std::uint32_t>(instruction.psrc1) << 22;
    word |= static_cast<std::uint32_t>(instruction.idx0) << 18;
    word |= static_cast<std::uint32_t>(instruction.idx1) << 14;
    word |= static_cast<std::uint32_t>(instruction.mode) << 10;
    word |= static_cast<std::uint32_t>(instruction.flag) << 7;
    word |= kCustom0Opcode;
    return word;
}

std::uint32_t encode_cfg(const Instruction& instruction) {
    ensure_range(instruction.idx0, 0, 7, "idx0");
    ensure_range(instruction.idx1, 0, 7, "idx1");
    if (instruction.cfg > 0x7FFF) {
        throw std::runtime_error("cfg is out of range");
    }
    if (instruction.mnemonic == Mnemonic::kPfree
        && (instruction.idx1 != 0 || instruction.cfg != 0)) {
        throw std::runtime_error("pfree reserved fields must be zero");
    }

    std::uint32_t word = 0;
    word |= opcode_for(instruction.mnemonic) << 28;
    word |= static_cast<std::uint32_t>(instruction.idx0) << 25;
    word |= static_cast<std::uint32_t>(instruction.idx1) << 22;
    word |= static_cast<std::uint32_t>(instruction.cfg) << 7;
    word |= kCustom0Opcode;
    return word;
}

std::uint32_t encode_sync(const Instruction& instruction) {
    ensure_range(instruction.tag, 0, 0x1F, "tag");
    ensure_range(instruction.mode, 0, 0x7, "mode");

    std::uint32_t word = 0;
    word |= opcode_for(instruction.mnemonic) << 28;
    word |= static_cast<std::uint32_t>(instruction.tag) << 23;
    word |= static_cast<std::uint32_t>(instruction.mode) << 20;
    word |= kCustom0Opcode;
    return word;
}

std::uint32_t encode_dma(const Instruction& instruction) {
    ensure_range(instruction.rs1, 0, 31, "rs1");
    ensure_range(instruction.rs2, 0, 31, "rs2");
    ensure_range(instruction.obj_id, 0, 7, "obj_id");
    ensure_range(instruction.type, 0, instruction.mnemonic == Mnemonic::kDstore ? 1 : 3, "type");

    const std::uint32_t dir = instruction.mnemonic == Mnemonic::kDstore ? 1U : 0U;

    std::uint32_t word = 0;
    word |= static_cast<std::uint32_t>(instruction.rs2) << 20;
    word |= static_cast<std::uint32_t>(instruction.rs1) << 15;
    word |= dir << 14;
    word |= static_cast<std::uint32_t>(instruction.type) << 12;
    word |= static_cast<std::uint32_t>(instruction.obj_id) << 9;
    word |= kCustom1Opcode;
    return word;
}

}  // namespace

std::uint32_t encode_instruction(const Instruction& instruction) {
    switch (instruction_format(instruction.mnemonic)) {
        case Format::kAR3:
            return encode_ar3(instruction);
        case Format::kSTG:
            return encode_stg(instruction);
        case Format::kCFG:
            return encode_cfg(instruction);
        case Format::kSYNC:
            return encode_sync(instruction);
        case Format::kDMA:
            return encode_dma(instruction);
    }

    throw std::runtime_error("unsupported instruction format");
}

}  // namespace hpu
