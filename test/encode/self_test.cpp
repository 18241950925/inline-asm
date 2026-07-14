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

} // namespace

int main()
{
    try {
        const std::string smoke =
            "dload x10, x11, p0, 0\n"
            "pmodld p4, 7, 32767\n"
            "padd p2, p0, p1\n"
            "paddi p2, p0, 255\n"
            "psub p2, p0, p1\n"
            "psubi p2, p0, 255\n"
            "pmul p2, p0, p1\n"
            "pmuli p2, p0, 255\n"
            "pmac p2, p0, p1\n"
            "pmaci p2, p0, 255\n"
            "pntt p0, p3, 15, 15, 15\n"
            "pintt p0, p3, 15, 15, 15\n"
            "pshcfg p5, 7, 32767\n"
            "pshuf p2, p0, 15, 15, 15\n"
            "pseed 2097151\n"
            "psample p2, p0, 15, 15, 15\n"
            "psync 31, 7\n"
            "dstore x10, x11, p2, 1\n";
        const auto encoded = hpu::assemble_source(smoke);
        if (encoded.size() != 18) {
            throw std::runtime_error("unexpected smoke instruction count");
        }
        if ((encoded.front().word & 0x7fU) != 0x2bU
            || (encoded.back().word & 0x7fU) != 0x2bU
            || (encoded[1].word & 0x7fU) != 0x0bU) {
            throw std::runtime_error("custom opcode routing mismatch");
        }

        const std::vector<std::string> invalid{
            "padd p8, p0, p1",
            "paddi p0, p1, 256",
            "pntt p0, p1, 16, 0, 0",
            "pmodld p0, 8, 0",
            "pseed 2097152",
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
