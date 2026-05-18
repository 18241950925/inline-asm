#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "assembler.hpp"

namespace {

const std::vector<std::string> kEncodableOutputs{
    "ntt",
    "intt",
    "mm",
    "bconv",
    "pmult",
    "modup",
    "moddown",
    "keyswitch",
};

const std::vector<std::string> kAllOutputs{
    "ntt",
    "intt",
    "mm",
    "bconv",
    "pmult",
    "cmult",
    "modup",
    "moddown",
    "auto",
    "keyswitch",
};

const std::vector<std::string> kSkippedOutputs{
    // auto.asm still contains symbolic DMA register placeholders such as
    // x_c0/x_offset/x_out, so it cannot be encoded before register allocation.
    "auto",
};

std::string read_all(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open input file: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string format_word_bits(std::uint32_t word) {
    std::string bits;
    bits.reserve(32);
    for (int bit = 31; bit >= 0; --bit) {
        bits.push_back(((word >> bit) & 1U) ? '1' : '0');
    }
    return bits;
}

void write_inst32(const std::filesystem::path& path,
                  const std::vector<hpu::EncodedInstruction>& encoded) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("failed to open output file: " + path.string());
    }

    for (const auto& item : encoded) {
        output << format_word_bits(item.word) << '\n';
    }
}

void package_case(const std::filesystem::path& source_root,
                  const std::filesystem::path& outputs_root,
                  const std::string& stem) {
    const auto case_dir = outputs_root / stem;
    std::filesystem::create_directories(case_dir / "test_data");

    for (const auto* extension : {".cpp", ".asm"}) {
        const auto source_path = source_root / (stem + extension);
        const auto target_path = case_dir / (stem + extension);
        if (!std::filesystem::exists(source_path)) {
            std::cout << "Skipped packaging " << source_path
                      << " (source file not generated)\n";
            continue;
        }

        std::filesystem::copy_file(
            source_path,
            target_path,
            std::filesystem::copy_options::overwrite_existing);
        std::cout << "Packaged " << source_path << " -> " << target_path << '\n';
    }
}

void encode_output(const std::filesystem::path& outputs_root,
                   const std::string& stem) {
    const auto case_dir = outputs_root / stem;
    const auto input_path = case_dir / (stem + ".asm");
    const auto output_path = case_dir / (stem + ".inst32");
    const auto encoded = hpu::assemble_source(read_all(input_path));
    write_inst32(output_path, encoded);
    std::cout << "Encoded " << input_path << " -> " << output_path
              << " (" << encoded.size() << " instructions)\n";
}

}  // namespace

int main() {
    try {
        const std::filesystem::path source_root{"output"};
        const std::filesystem::path outputs_root{"outputs"};

        for (const auto& stem : kAllOutputs) {
            package_case(source_root, outputs_root, stem);
        }

        for (const auto& stem : kEncodableOutputs) {
            encode_output(outputs_root, stem);
        }

        for (const auto& stem : kSkippedOutputs) {
            std::cout << "Skipped " << outputs_root / stem / (stem + ".asm")
                      << " (contains symbolic DMA register placeholders)\n";
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
