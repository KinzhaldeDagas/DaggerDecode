#pragma once
#include "../pch.h"

namespace arena2 {

struct IndexCatalog {
    struct Span {
        uint16_t a{};
        uint16_t b{};
        std::wstring label;
    };

    std::vector<Span> spans;
    std::vector<std::wstring> labelOrder;

    bool LoadFromFile(const std::filesystem::path& path, std::wstring* err);
    std::wstring_view LabelFor(uint16_t id) const;

private:
    static std::wstring Trim(std::wstring s);
    static std::wstring StripParens(std::wstring s);
    static bool StartsWithDigit(const std::wstring& s);
    static bool ParseIndexList(const std::wstring& s, std::vector<std::pair<uint16_t,uint16_t>>& out);
};

} // namespace arena2
