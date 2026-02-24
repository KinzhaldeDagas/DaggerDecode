#include "pch.h"
#include "VarHashCatalog.h"
#include "../util/WinUtil.h"

namespace arena2 {

static uint32_t ParseHex32(const std::string& s) {
    uint32_t v = 0;
    for (char c : s) {
        uint8_t d = 0xFF;
        if (c >= '0' && c <= '9') d = (uint8_t)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (uint8_t)(10 + (c - 'a'));
        else if (c >= 'A' && c <= 'F') d = (uint8_t)(10 + (c - 'A'));
        else continue;
        v = (v << 4) | d;
    }
    return v;
}

static std::string ToLowerAscii(std::string s) {
    for (auto& c : s) {
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    }
    return s;
}

uint32_t ComputeVarHash(std::string_view nameLowerAscii) {
    // UESP: val = (val<<1) + *ptr for each character byte.
    uint32_t val = 0;
    for (unsigned char c : nameLowerAscii) {
        val = (val << 1) + (uint32_t)c;
    }
    return val;
}

bool VarHashCatalog::LoadFromFile(const std::filesystem::path& path, std::wstring* err) {
    hashToNames.clear();

    std::ifstream f(path, std::ios::binary);
    if (!f) { if (err) *err = L"Failed to open TEXT_VARIABLE_HASHES.txt"; return false; }

    std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    // The wiki export isn't strictly structured; parse any line containing "0x" + 8 hex + name token.
    size_t pos = 0;
    while (pos < bytes.size()) {
        size_t eol = bytes.find('\n', pos);
        std::string line = (eol == std::string::npos) ? bytes.substr(pos) : bytes.substr(pos, eol - pos);
        pos = (eol == std::string::npos) ? bytes.size() : (eol + 1);

        auto p = line.find("0x");
        if (p == std::string::npos) continue;
        if (p + 10 > line.size()) continue;

        std::string hex = line.substr(p + 2, 8);
        bool ok = true;
        for (char c : hex) {
            bool isHex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            if (!isHex) { ok = false; break; }
        }
        if (!ok) continue;

        // After hex, expect whitespace then variable name token
        size_t q = p + 10;
        while (q < line.size() && (line[q] == ' ' || line[q] == '\t')) q++;
        if (q >= line.size()) continue;

        // name token until whitespace
        size_t r = q;
        while (r < line.size() && line[r] != ' ' && line[r] != '\t' && line[r] != '\r') r++;
        std::string name = line.substr(q, r - q);
        if (name.empty()) continue;

        // Normalize
        name = ToLowerAscii(name);

        uint32_t hv = ParseHex32(hex);
        auto& vec = hashToNames[hv];
        bool exists = false;
        for (auto& n : vec) {
            if (_stricmp(n.c_str(), name.c_str()) == 0) { exists = true; break; }
        }
        if (!exists) vec.push_back(std::move(name));
    }

    if (hashToNames.empty()) {
        if (err) *err = L"VarHashCatalog loaded but produced 0 hash entries.";
        return false;
    }
    return true;
}

} // namespace arena2
