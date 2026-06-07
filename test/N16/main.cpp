#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "assembler.hpp"
#include "util/mm.hpp"
#include "util/ntt.hpp"

namespace {

constexpr std::size_t kLogN = 16;
constexpr std::size_t kN = std::size_t{1} << kLogN;

constexpr int kObjA = 0;
constexpr int kObjB = 1;
constexpr int kObjOut = 2;
constexpr int kObjTwiddle = 3;
constexpr int kObjModCtx = 4;

struct ModulusConfig {
    std::uint32_t modulus;
    std::uint32_t psi;
};

constexpr std::array<ModulusConfig, 2> kModuli{{
    {4293918721U, 2928850483U},
    {4291952641U, 174141454U},
}};

std::uint32_t mod_pow(std::uint32_t base,
                      std::uint64_t exponent,
                      std::uint32_t modulus) {
    std::uint64_t result = 1;
    std::uint64_t value = base;
    while (exponent != 0) {
        if ((exponent & 1U) != 0) {
            result = (result * value) % modulus;
        }
        value = (value * value) % modulus;
        exponent >>= 1U;
    }
    return static_cast<std::uint32_t>(result);
}

std::uint32_t mul_mod(std::uint32_t lhs,
                      std::uint32_t rhs,
                      std::uint32_t modulus) {
    return static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(lhs) * rhs) % modulus);
}

std::uint32_t add_mod(std::uint32_t lhs,
                      std::uint32_t rhs,
                      std::uint32_t modulus) {
    const std::uint64_t sum = static_cast<std::uint64_t>(lhs) + rhs;
    return static_cast<std::uint32_t>(sum % modulus);
}

std::uint32_t sub_mod(std::uint32_t lhs,
                      std::uint32_t rhs,
                      std::uint32_t modulus) {
    return lhs >= rhs ? lhs - rhs : modulus - (rhs - lhs);
}

std::uint32_t normalize_signed(std::int64_t value, std::uint32_t modulus) {
    std::int64_t residue = value % static_cast<std::int64_t>(modulus);
    if (residue < 0) {
        residue += modulus;
    }
    return static_cast<std::uint32_t>(residue);
}

void validate_modulus(const ModulusConfig& config) {
    constexpr std::uint64_t kRootOrder = 2 * kN;
    if ((static_cast<std::uint64_t>(config.modulus) - 1) % kRootOrder != 0) {
        throw std::runtime_error("modulus does not support N=65536");
    }
    if (mod_pow(config.psi, kRootOrder, config.modulus) != 1 ||
        mod_pow(config.psi, kN, config.modulus) != config.modulus - 1) {
        throw std::runtime_error("invalid primitive 2N root");
    }
}

std::vector<std::uint32_t> make_input_a(std::uint32_t modulus) {
    std::vector<std::uint32_t> values(kN);
    for (std::size_t i = 0; i < kN; ++i) {
        const std::int64_t signed_value =
            static_cast<std::int64_t>((17 * i + 3) % 257) - 128;
        values[i] = normalize_signed(signed_value, modulus);
    }
    return values;
}

std::vector<std::uint32_t> make_input_b(std::uint32_t modulus) {
    std::vector<std::uint32_t> values(kN);
    for (std::size_t i = 0; i < kN; ++i) {
        const std::int64_t signed_value =
            static_cast<std::int64_t>((29 * i + 5) % 263) - 131;
        values[i] = normalize_signed(signed_value, modulus);
    }
    return values;
}

std::vector<std::uint32_t> pointwise_mm(
    const std::vector<std::uint32_t>& lhs,
    const std::vector<std::uint32_t>& rhs,
    std::uint32_t modulus) {
    std::vector<std::uint32_t> result(kN);
    for (std::size_t i = 0; i < kN; ++i) {
        result[i] = mul_mod(lhs[i], rhs[i], modulus);
    }
    return result;
}

void bit_reverse(std::vector<std::uint32_t>& values) {
    for (std::size_t i = 1, j = 0; i < values.size(); ++i) {
        std::size_t bit = values.size() >> 1;
        while ((j & bit) != 0) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j) {
            std::swap(values[i], values[j]);
        }
    }
}

std::vector<std::uint32_t> ckks_ntt(
    const std::vector<std::uint32_t>& input,
    const ModulusConfig& config) {
    std::vector<std::uint32_t> values(kN);
    std::uint32_t twist = 1;
    for (std::size_t i = 0; i < kN; ++i) {
        values[i] = mul_mod(input[i], twist, config.modulus);
        twist = mul_mod(twist, config.psi, config.modulus);
    }

    bit_reverse(values);
    const std::uint32_t omega =
        mul_mod(config.psi, config.psi, config.modulus);
    for (std::size_t width = 2; width <= kN; width <<= 1) {
        const std::uint32_t step =
            mod_pow(omega, kN / width, config.modulus);
        for (std::size_t base = 0; base < kN; base += width) {
            std::uint32_t twiddle = 1;
            for (std::size_t j = 0; j < width / 2; ++j) {
                const std::uint32_t lhs = values[base + j];
                const std::uint32_t rhs =
                    mul_mod(values[base + j + width / 2],
                            twiddle,
                            config.modulus);
                values[base + j] = add_mod(lhs, rhs, config.modulus);
                values[base + j + width / 2] =
                    sub_mod(lhs, rhs, config.modulus);
                twiddle = mul_mod(twiddle, step, config.modulus);
            }
        }
    }
    return values;
}

std::vector<std::uint32_t> stage_twiddles(
    const ModulusConfig& config,
    std::size_t stage) {
    const std::size_t width = std::size_t{1} << (stage + 1);
    const std::uint32_t omega =
        mul_mod(config.psi, config.psi, config.modulus);
    const std::uint32_t step =
        mod_pow(omega, kN / width, config.modulus);

    std::vector<std::uint32_t> values(width / 2, 1);
    for (std::size_t i = 1; i < values.size(); ++i) {
        values[i] = mul_mod(values[i - 1], step, config.modulus);
    }
    return values;
}

void write_u32(std::ofstream& output, std::uint32_t value) {
    const std::array<unsigned char, 4> bytes{{
        static_cast<unsigned char>(value),
        static_cast<unsigned char>(value >> 8),
        static_cast<unsigned char>(value >> 16),
        static_cast<unsigned char>(value >> 24),
    }};
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
}

void write_magic(std::ofstream& output, const std::array<char, 8>& magic) {
    output.write(magic.data(), static_cast<std::streamsize>(magic.size()));
}

void write_vector(std::ofstream& output,
                  const std::vector<std::uint32_t>& values) {
    for (const auto value : values) {
        write_u32(output, value);
    }
}

void write_common_header(std::ofstream& output,
                         const std::array<char, 8>& magic) {
    write_magic(output, magic);
    write_u32(output, 1);
    write_u32(output, static_cast<std::uint32_t>(kLogN));
    write_u32(output, static_cast<std::uint32_t>(kN));
    write_u32(output, static_cast<std::uint32_t>(kModuli.size()));
    write_u32(output, 32);
}

void write_mm_data(const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("failed to open mm_data.bin");
    }
    write_common_header(output, {{'H', 'P', 'U', 'M', 'M', '1', '6', '\0'}});

    for (const auto& config : kModuli) {
        const auto lhs = make_input_a(config.modulus);
        const auto rhs = make_input_b(config.modulus);
        const auto expected = pointwise_mm(lhs, rhs, config.modulus);

        write_u32(output, config.modulus);
        write_u32(output, config.psi);
        write_u32(
            output,
            mul_mod(config.psi, config.psi, config.modulus));
        write_vector(output, lhs);
        write_vector(output, rhs);
        write_vector(output, expected);
    }
}

void write_ntt_data(const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("failed to open ntt_data.bin");
    }
    write_common_header(output, {{'H', 'P', 'U', 'N', 'T', '1', '6', '\0'}});

    for (const auto& config : kModuli) {
        const auto input = make_input_a(config.modulus);
        const auto expected = ckks_ntt(input, config);

        write_u32(output, config.modulus);
        write_u32(output, config.psi);
        write_u32(
            output,
            mul_mod(config.psi, config.psi, config.modulus));
        write_vector(output, input);
        write_vector(output, expected);
        write_u32(output, static_cast<std::uint32_t>(kN - 1));
        for (std::size_t stage = 0; stage < kLogN; ++stage) {
            const auto twiddles = stage_twiddles(config, stage);
            write_u32(output, static_cast<std::uint32_t>(stage));
            write_u32(output, static_cast<std::uint32_t>(twiddles.size()));
            write_vector(output, twiddles);
        }
    }
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("failed to open " + path.string());
    }
    output << text;
}

void write_inst32(const std::filesystem::path& path,
                  const std::string& generated_cpp) {
    const auto encoded = hpu::assemble_source(generated_cpp);
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("failed to open " + path.string());
    }
    for (const auto& instruction : encoded) {
        for (int bit = 31; bit >= 0; --bit) {
            output << (((instruction.word >> bit) & 1U) ? '1' : '0');
        }
        output << '\n';
    }
}

void write_readme(const std::filesystem::path& path) {
    std::ostringstream output;
    output << "# N16测试\n\n";
    output << "`N16`表示`logN=16`,实际多项式长度为`N=65536`。"
              "系数和RNS模数均按32bit存储。\n\n";
    output << "## 文件\n\n";
    output << "- `mm.cpp`:原`generate_hpu_mm_asm()`生成的内联汇编。\n";
    output << "- `mm.inst32`:MM的32bit指令编码。\n";
    output << "- `mm_data.bin`:MM输入A、输入B和期望输出。\n";
    output << "- `ntt.cpp`:原`generate_hpu_ntt_asm(N=65536)`生成的内联汇编。\n";
    output << "- `ntt.inst32`:NTT的32bit指令编码,包含16个stage。\n";
    output << "- `ntt_data.bin`:NTT输入、期望输出和16个stage的twiddle。\n\n";
    output << "## 公共文件头\n\n";
    output << "全部整数使用little-endian。\n\n";
    output << "`magic[8],version:u32,logN:u32,N:u32,L:u32,coeff_bits:u32`\n\n";
    output << "当前值:`logN=16,N=65536,L=2,coeff_bits=32`。\n\n";
    output << "## MM数据布局\n\n";
    output << "每个RNSlimb依次保存:"
              "`q:u32,psi:u32,omega:u32,A[N],B[N],expected[N]`。\n\n";
    output << "## NTT数据布局\n\n";
    output << "每个RNSlimb依次保存:"
              "`q:u32,psi:u32,omega:u32,input[N],expected[N],twiddle_count:u32`，"
              "随后保存16组`stage:u32,count:u32,twiddle[count]`。\n\n";
    output << "模数为`4293918721`和`4291952641`，"
              "两者都支持`2×65536`阶单位根。"
              "MM执行前需由测试平台把当前limb的A、B、模上下文装入p0、p1、p4。"
              "q0和q1分别执行同一份指令。\n";
    write_text(path, output.str());
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const std::filesystem::path output_dir =
            argc > 1 ? argv[1] : "test/N16/generated";
        std::filesystem::create_directories(output_dir);

        for (const auto& config : kModuli) {
            validate_modulus(config);
        }

        const std::string mm_cpp = generate_hpu_mm_asm(
            kObjOut,
            kObjA,
            kObjB,
            kObjModCtx,
            true);
        const std::string ntt_cpp = generate_hpu_ntt_asm(
            static_cast<int>(kN),
            kObjA,
            kObjTwiddle,
            kObjModCtx,
            true);

        write_text(output_dir / "mm.cpp", mm_cpp);
        write_inst32(output_dir / "mm.inst32", mm_cpp);
        write_mm_data(output_dir / "mm_data.bin");

        write_text(output_dir / "ntt.cpp", ntt_cpp);
        write_inst32(output_dir / "ntt.inst32", ntt_cpp);
        write_ntt_data(output_dir / "ntt_data.bin");

        write_readme(output_dir / "README.md");
        std::cout << "Generated compact N16 tests under " << output_dir << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
