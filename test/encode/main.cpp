#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
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

void write_inst32(const std::filesystem::path& path,
                  const std::vector<hpu::EncodedInstruction>& encoded);

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

std::uint32_t mod_pow(std::uint32_t base, std::uint32_t exp, std::uint32_t mod) {
    std::uint64_t result = 1;
    std::uint64_t value = base % mod;
    while (exp != 0) {
        if ((exp & 1U) != 0) {
            result = (result * value) % mod;
        }
        value = (value * value) % mod;
        exp >>= 1U;
    }
    return static_cast<std::uint32_t>(result);
}

std::string hex32(std::uint32_t value) {
    std::ostringstream ss;
    ss << "0x" << std::uppercase << std::hex << std::setw(8)
       << std::setfill('0') << value;
    return ss.str();
}

void write_text_file(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("failed to open output file: " + path.string());
    }
    output << text;
}

void write_vector_csv(const std::filesystem::path& path,
                      const std::vector<std::uint32_t>& values,
                      const std::string& value_name) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("failed to open output file: " + path.string());
    }

    output << "index," << value_name << "_dec," << value_name << "_hex,width\n";
    for (std::size_t i = 0; i < values.size(); ++i) {
        output << i << ',' << values[i] << ',' << hex32(values[i]) << ",32b\n";
    }
}

std::vector<std::uint32_t> direct_ntt(const std::vector<std::uint32_t>& input,
                                      std::uint32_t modulus,
                                      std::uint32_t root) {
    const std::size_t n = input.size();
    std::vector<std::uint32_t> output(n, 0);
    for (std::size_t k = 0; k < n; ++k) {
        std::uint64_t acc = 0;
        for (std::size_t j = 0; j < n; ++j) {
            const auto twiddle = mod_pow(root, static_cast<std::uint32_t>((j * k) % n), modulus);
            acc = (acc + static_cast<std::uint64_t>(input[j]) * twiddle) % modulus;
        }
        output[k] = static_cast<std::uint32_t>(acc);
    }
    return output;
}

std::vector<std::uint32_t> stage_twiddles(std::size_t n,
                                          int stage,
                                          std::uint32_t modulus,
                                          std::uint32_t root) {
    const std::size_t m = std::size_t{1} << (stage + 1);
    const auto step_root = mod_pow(root, static_cast<std::uint32_t>(n / m), modulus);
    std::vector<std::uint32_t> twiddles(m / 2, 1);
    for (std::size_t j = 1; j < twiddles.size(); ++j) {
        twiddles[j] = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(twiddles[j - 1]) * step_root) % modulus);
    }
    return twiddles;
}

void write_ntt_small_test_data(const std::filesystem::path& test_data_dir) {
    std::filesystem::create_directories(test_data_dir);

    constexpr std::size_t kN = 8;
    constexpr std::uint32_t kModulus = 17;
    constexpr std::uint32_t kRoot = 9;
    constexpr int kPolyObj = 0;
    constexpr int kTwiddleObj = 1;
    constexpr int kModCtxObj = 2;

    const std::vector<std::uint32_t> input{1, 2, 3, 4, 5, 6, 7, 8};
    const auto expected = direct_ntt(input, kModulus, kRoot);

    write_vector_csv(test_data_dir / "input_coeffs.csv", input, "coeff");
    write_vector_csv(test_data_dir / "expected_ntt.csv", expected, "coeff");

    {
        std::ofstream output(test_data_dir / "mod_ctx.csv");
        if (!output) {
            throw std::runtime_error("failed to open output file: " +
                                     (test_data_dir / "mod_ctx.csv").string());
        }
        output << "field,value,value_hex_or_obj,width\n";
        output << "N," << kN << ',' << hex32(static_cast<std::uint32_t>(kN)) << ",parameter\n";
        output << "modulus_q," << kModulus << ',' << hex32(kModulus) << ",32b\n";
        output << "omega," << kRoot << ',' << hex32(kRoot) << ",32b\n";
        output << "poly_obj,p" << kPolyObj << ",p" << kPolyObj << ",3b\n";
        output << "twiddle_obj,p" << kTwiddleObj << ",p" << kTwiddleObj << ",3b\n";
        output << "mod_ctx_obj,p" << kModCtxObj << ",p" << kModCtxObj << ",3b\n";
    }

    for (int stage = 0; stage < 3; ++stage) {
        write_vector_csv(
            test_data_dir / ("twiddle_stage_" + std::to_string(stage) + ".csv"),
            stage_twiddles(kN, stage, kModulus, kRoot),
            "twiddle");
    }

    std::ostringstream asm_text;
    asm_text << "dload x0, x0, p0, 1\n";
    asm_text << "dload x0, x0, p2, 2\n";
    asm_text << "pmodld p2, 0, 0\n";
    for (int stage = 0; stage < 3; ++stage) {
        asm_text << "dload x0, x0, p1, 1\n";
        asm_text << "pntt p0, p1, " << stage << ", 0, 0\n";
    }
    asm_text << "psync 0, 0\n";
    const auto standalone_asm = asm_text.str();
    write_text_file(test_data_dir / "standalone_ntt.asm", standalone_asm);
    write_inst32(
        test_data_dir / "standalone_ntt.inst32",
        hpu::assemble_source(standalone_asm));

    std::ostringstream manifest;
    manifest << "# NTT小测试数据\n\n";
    manifest << "- 用途:验证独立`ntt.asm`在执行`pntt`前会先加载模上下文。\n";
    manifest << "- 参数:N=8,q=17,omega=9,输入系数为1..8。\n";
    manifest << "- 槽位:p0存放输入/输出多项式,p1存放每个stage的twiddle,p2存放mod_ctx。\n";
    manifest << "- 位宽:指令=32b,custom0payload=25b,对象槽=3b,stage=4b,idx1=4b,mode=4b,系数=32b,模数=32b。\n";
    manifest << "- `standalone_ntt.asm`是与本fixture匹配的最小指令序列,开头会加载p0输入和p2模上下文。\n\n";
    manifest << "## 文件\n\n";
    manifest << "- `input_coeffs.csv`:输入多项式系数。\n";
    manifest << "- `expected_ntt.csv`:按直接NTT公式计算的期望输出。\n";
    manifest << "- `mod_ctx.csv`:小测试使用的模上下文参数。\n";
    manifest << "- `twiddle_stage_*.csv`:每个stage使用的twiddle参考值。\n";
    manifest << "- `standalone_ntt.asm`:包含`dload mod_ctx`和`pmodld`的测试指令。\n";
    manifest << "- `standalone_ntt.inst32`:由`standalone_ntt.asm`编码得到的32位指令串。\n";
    write_text_file(test_data_dir / "manifest.md", manifest.str());
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

    if (stem == "ntt") {
        write_ntt_small_test_data(case_dir / "test_data");
        std::cout << "Generated small NTT test data under "
                  << case_dir / "test_data" << '\n';
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
