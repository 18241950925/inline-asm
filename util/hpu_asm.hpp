#pragma once

#include <string>
#include <sstream>

namespace hpu {

inline std::string pobj(int id) {
    return "p" + std::to_string(id);
}

// ==========================================
// 外部访存类指令 (custom1)
// ==========================================
inline std::string dload(const std::string& rs1, const std::string& rs2, int pdst, int load_type) {
    std::ostringstream ss;
    ss << "        \"dload " << rs1 << ", " << rs2 << ", " << pobj(pdst) << ", " << load_type << " \\n\\t\"\n";
    return ss.str();
}

inline std::string dstore(const std::string& rs1, const std::string& rs2, int psrc, int rel) {
    std::ostringstream ss;
    ss << "        \"dstore " << rs1 << ", " << rs2 << ", " << pobj(psrc) << ", " << rel << " \\n\\t\"\n";
    return ss.str();
}

// ==========================================
// 内部执行类指令 (custom0)
// ==========================================

// --- AR3 格式：三对象基础算术类 ---
// 包含 pdst, psrc1, OP2(psrc2或cimm8), MODE 共4个显式操作数字段
inline std::string padd(int pdst, int psrc1, int psrc2) {
    std::ostringstream ss;
    ss << "        \"padd " << pobj(pdst) << ", " << pobj(psrc1) << ", " << pobj(psrc2) << " \\n\\t\"\n";
    return ss.str();
}

inline std::string paddi(int pdst, int psrc1, int cimm8) {
    std::ostringstream ss;
    ss << "        \"paddi " << pobj(pdst) << ", " << pobj(psrc1) << ", " << cimm8 << " \\n\\t\"\n";
    return ss.str();
}

inline std::string psub(int pdst, int psrc1, int psrc2) {
    std::ostringstream ss;
    ss << "        \"psub " << pobj(pdst) << ", " << pobj(psrc1) << ", " << pobj(psrc2) << " \\n\\t\"\n";
    return ss.str();
}

inline std::string psubi(int pdst, int psrc1, int cimm8) {
    std::ostringstream ss;
    ss << "        \"psubi " << pobj(pdst) << ", " << pobj(psrc1) << ", " << cimm8 << " \\n\\t\"\n";
    return ss.str();
}

inline std::string pmul(int pdst, int psrc1, int psrc2) {
    std::ostringstream ss;
    ss << "        \"pmul " << pobj(pdst) << ", " << pobj(psrc1) << ", " << pobj(psrc2) << " \\n\\t\"\n";
    return ss.str();
}

inline std::string pmuli(int pdst, int psrc1, int cimm8) {
    std::ostringstream ss;
    ss << "        \"pmuli " << pobj(pdst) << ", " << pobj(psrc1) << ", " << cimm8 << " \\n\\t\"\n";
    return ss.str();
}

inline std::string pmac(int pdst, int psrc1, int psrc2) {
    std::ostringstream ss;
    ss << "        \"pmac " << pobj(pdst) << ", " << pobj(psrc1) << ", " << pobj(psrc2) << " \\n\\t\"\n";
    return ss.str();
}

inline std::string pmaci(int pdst, int psrc1, int cimm8) {
    std::ostringstream ss;
    ss << "        \"pmaci " << pobj(pdst) << ", " << pobj(psrc1) << ", " << cimm8 << " \\n\\t\"\n";
    return ss.str();
}

// --- STG 格式：stage / transform 执行类 ---
inline std::string pntt(int pdst, int psrc, int stage, int idx1, int mode = 0) {
    std::ostringstream ss;
    ss << "        \"pntt " << pobj(pdst) << ", " << pobj(psrc) << ", " << stage << ", " << idx1 << ", " << mode << " \\n\\t\"\n";
    return ss.str();
}

inline std::string pintt(int pdst, int psrc, int stage, int idx1, int mode = 0) {
    std::ostringstream ss;
    ss << "        \"pintt " << pobj(pdst) << ", " << pobj(psrc) << ", " << stage << ", " << idx1 << ", " << mode << " \\n\\t\"\n";
    return ss.str();
}

inline std::string pshuf(int pdst, int psrc, int idx0, int idx1, int mode = 0) {
    std::ostringstream ss;
    ss << "        \"pshuf " << pobj(pdst) << ", " << pobj(psrc) << ", " << idx0 << ", " << idx1 << ", " << mode << " \\n\\t\"\n";
    return ss.str();
}

inline std::string psample(int pdst, int psrc, int idx0, int idx1, int mode = 0) {
    std::ostringstream ss;
    ss << "        \"psample " << pobj(pdst) << ", " << pobj(psrc) << ", " << idx0 << ", " << idx1 << ", " << mode << " \\n\\t\"\n";
    return ss.str();
}

// --- CFG 格式：配置 / 控制类 ---
inline std::string pshcfg(int idx0, int idx1 = 0, int cfg = 0) {
    std::ostringstream ss;
    ss << "        \"pshcfg " << pobj(idx0) << ", " << idx1 << ", " << cfg << " \\n\\t\"\n";
    return ss.str();
}

inline std::string pmodld(int idx0, int idx1 = 0, int cfg = 0) {
    std::ostringstream ss;
    ss << "        \"pmodld " << pobj(idx0) << ", " << idx1 << ", " << cfg << " \\n\\t\"\n";
    return ss.str();
}

inline std::string pseed(int imm21) {
    std::ostringstream ss;
    ss << "        \"pseed " << imm21 << " \\n\\t\"\n";
    return ss.str();
}

// --- SYNC 格式：同步屏障类 ---
inline std::string psync(int tag, int mode = 0) {
    std::ostringstream ss;
    ss << "        \"psync " << tag << ", " << mode << " \\n\\t\"\n";
    return ss.str();
}

} // namespace hpu
