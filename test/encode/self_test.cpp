#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "assembler.hpp"

namespace {

void expect_rejected(const std::string& source)
{
    try {
        (void)hpu::assemble_line(source);
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error("invalid instruction was accepted: " + source);
}

void expect_encoded(const std::string& source, std::uint32_t expected)
{
    const hpu::EncodedInstruction encoded = hpu::assemble_line(source);
    if (encoded.word != expected) {
        throw std::runtime_error("unexpected encoding for: " + source);
    }
}

} // namespace

int main()
{
    try {
        const std::string smoke =
            "dload x10, x11, p0, 0\n"
            "pmodld p4, 7, 32767\n"
            "padd p2, p0, p1\n"
            "psub p2, p0, p1\n"
            "pmul p2, p0, p1\n"
            "pmac p2, p0, p1\n"
            "pntt p0, p3, 15, 15, 15\n"
            "pintt p0, p3, 15, 15, 15\n"
            "pfree p5\n"
            "psync 31, 7\n"
            "dstore x10, x11, p2, 1\n";
        const auto encoded = hpu::assemble_source(smoke);
        if (encoded.size() != 11) {
            throw std::runtime_error("unexpected smoke instruction count");
        }
        if ((encoded.front().word & 0x7fU) != 0x2bU
            || (encoded.back().word & 0x7fU) != 0x2bU
            || (encoded[1].word & 0x7fU) != 0x0bU) {
            throw std::runtime_error("custom opcode routing mismatch");
        }

        expect_encoded("padd p2, p0, p1", 0x0400400BU);
        expect_encoded("psub p2, p0, p1", 0x1400400BU);
        expect_encoded("pmul p2, p0, p1", 0x2400400BU);
        expect_encoded("pmac p2, p0, p1", 0x3400400BU);
        expect_encoded("pntt p0, p3, 15, 0, 0", 0x40FC000BU);
        expect_encoded("pintt p0, p3, 15, 0, 0", 0x50FC000BU);
        expect_encoded("pmodld p4, 0, 0", 0x6800000BU);
        expect_encoded("pfree p4", 0x7800000BU);
        expect_encoded("psync 0, 0", 0x8000000BU);
        expect_encoded("dload x10, x11, p0, 0", 0x00B5002BU);
        expect_encoded("dstore x10, x11, p2, 1", 0x00B5542BU);
        expect_encoded("pmul p2, p0, 255", 0x243FC40BU);
        expect_encoded("pmac p2, p0, 255", 0x343FC40BU);

        const std::vector<std::string> invalid{
            "padd p8, p0, p1",
            "padd p0, p1, 1",
            "pmul p0, p1, 256",
            "pntt p0, p1, 16, 0, 0",
            "pmodld p0, 8, 0",
            "pfree p8",
            "pfree p0, 0, 0",
            "pshcfg p0, 0, 0",
            "pshuf p0, p1, 0, 0, 0",
            "pseed 0",
            "psample p0, p1, 0, 0, 0",
            "psync 32, 0",
            "dload x32, x0, p0, 0",
            "dload x0, x0, p0, 4",
            "dstore x0, x0, p0, 2",
        };
        for (const std::string& source : invalid) {
            expect_rejected(source);
        }

        std::cout << "HPU encoder self-test: PASS\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "HPU encoder self-test failed: " << exception.what() << '\n';
        return 1;
    }
}
