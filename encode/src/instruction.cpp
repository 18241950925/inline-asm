#include "instruction.hpp"

#include <sstream>
#include <stdexcept>

namespace hpu {
namespace {

std::string format_pobj(int value) {
    return "p" + std::to_string(value);
}

std::string format_xreg(int value) {
    return "x" + std::to_string(value);
}

bool is_immediate_ar3(Mnemonic mnemonic) {
    return mnemonic == Mnemonic::kPaddi || mnemonic == Mnemonic::kPsubi ||
           mnemonic == Mnemonic::kPmuli || mnemonic == Mnemonic::kPmaci;
}

}  // namespace

std::string to_string(Mnemonic mnemonic) {
    switch (mnemonic) {
        case Mnemonic::kPadd: return "padd";
        case Mnemonic::kPaddi: return "paddi";
        case Mnemonic::kPsub: return "psub";
        case Mnemonic::kPsubi: return "psubi";
        case Mnemonic::kPmul: return "pmul";
        case Mnemonic::kPmuli: return "pmuli";
        case Mnemonic::kPmac: return "pmac";
        case Mnemonic::kPmaci: return "pmaci";
        case Mnemonic::kPntt: return "pntt";
        case Mnemonic::kPintt: return "pintt";
        case Mnemonic::kPshuf: return "pshuf";
        case Mnemonic::kPsample: return "psample";
        case Mnemonic::kPshcfg: return "pshcfg";
        case Mnemonic::kPseed: return "pseed";
        case Mnemonic::kPmodld: return "pmodld";
        case Mnemonic::kPsync: return "psync";
        case Mnemonic::kDload: return "dload";
        case Mnemonic::kDstore: return "dstore";
    }

    throw std::runtime_error("unknown mnemonic");
}

Format instruction_format(Mnemonic mnemonic) {
    switch (mnemonic) {
        case Mnemonic::kPadd:
        case Mnemonic::kPaddi:
        case Mnemonic::kPsub:
        case Mnemonic::kPsubi:
        case Mnemonic::kPmul:
        case Mnemonic::kPmuli:
        case Mnemonic::kPmac:
        case Mnemonic::kPmaci:
            return Format::kAR3;

        case Mnemonic::kPntt:
        case Mnemonic::kPintt:
        case Mnemonic::kPshuf:
        case Mnemonic::kPsample:
            return Format::kSTG;

        case Mnemonic::kPshcfg:
        case Mnemonic::kPseed:
        case Mnemonic::kPmodld:
            return Format::kCFG;

        case Mnemonic::kPsync:
            return Format::kSYNC;

        case Mnemonic::kDload:
        case Mnemonic::kDstore:
            return Format::kDMA;
    }

    throw std::runtime_error("unknown format for mnemonic");
}

std::string to_string(const Instruction& instruction) {
    std::ostringstream oss;
    oss << to_string(instruction.mnemonic);

    switch (instruction_format(instruction.mnemonic)) {
        case Format::kAR3:
            oss << ' ' << format_pobj(instruction.pdst)
                << ", " << format_pobj(instruction.psrc1)
                << ", ";
            if (is_immediate_ar3(instruction.mnemonic)) {
                oss << instruction.imm8;
            } else {
                oss << format_pobj(instruction.psrc2);
            }
            break;

        case Format::kSTG:
            oss << ' ' << format_pobj(instruction.pdst)
                << ", " << format_pobj(instruction.psrc1)
                << ", " << instruction.idx0
                << ", " << instruction.idx1
                << ", " << static_cast<int>(instruction.mode);
            break;

        case Format::kCFG:
            if (instruction.mnemonic == Mnemonic::kPseed) {
                oss << ' ' << instruction.imm21;
            } else {
                oss << ' ' << format_pobj(instruction.idx0)
                    << ", " << instruction.idx1
                    << ", " << instruction.cfg;
            }
            break;

        case Format::kSYNC:
            oss << ' ' << static_cast<int>(instruction.tag)
                << ", " << static_cast<int>(instruction.mode);
            break;

        case Format::kDMA:
            oss << ' ' << format_xreg(instruction.rs1)
                << ", " << format_xreg(instruction.rs2)
                << ", " << format_pobj(instruction.obj_id)
                << ", " << static_cast<int>(instruction.type);
            break;
    }

    return oss.str();
}

}  // namespace hpu
