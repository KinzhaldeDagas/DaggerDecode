#include "pch.h"
#include "TextRscIndex.h"
#include "../util/WinUtil.h"

namespace arena2 {

static uint16_t ParseU16_4(const std::wstring& tok) {
    std::wstring t;
    t.reserve(4);
    for (wchar_t c : tok) {
        if (iswdigit(c)) t.push_back(c);
    }
    if (t.empty()) return 0;
    if (t.size() > 4) t = t.substr(t.size() - 4);
    while (t.size() < 4) t.insert(t.begin(), L'0');
    return (uint16_t)std::stoi(t);
}

std::wstring IndexCatalog::Trim(std::wstring s) {
    auto isSpace = [](wchar_t c) { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n'; };
    while (!s.empty() && isSpace(s.front())) s.erase(s.begin());
    while (!s.empty() && isSpace(s.back())) s.pop_back();
    return s;
}

std::wstring IndexCatalog::StripParens(std::wstring s) {
    // Remove trailing parenthetical notes "(...)" (common in the index file).
    // Keep inner apostrophes/quotes in main text.
    size_t p = s.find(L'(');
    if (p != std::wstring::npos) {
        s = Trim(s.substr(0, p));
    }
    return s;
}

bool IndexCatalog::StartsWithDigit(const std::wstring& s) {
    for (wchar_t c : s) {
        if (c == L' ' || c == L'\t') continue;
        return iswdigit(c) != 0;
    }
    return false;
}

bool IndexCatalog::ParseIndexList(const std::wstring& s, std::vector<std::pair<uint16_t,uint16_t>>& out) {
    out.clear();

    std::wstring t = s;
    // normalize: drop braces
    for (auto& c : t) {
        if (c == L'{' || c == L'}') c = L' ';
    }
    t = Trim(t);

    // split by commas
    size_t i = 0;
    while (i < t.size()) {
        size_t j = t.find(L',', i);
        std::wstring seg = (j == std::wstring::npos) ? t.substr(i) : t.substr(i, j - i);
        seg = Trim(seg);
        if (!seg.empty()) {
            size_t dash = seg.find(L'-');
            if (dash != std::wstring::npos) {
                std::wstring a = Trim(seg.substr(0, dash));
                std::wstring b = Trim(seg.substr(dash + 1));
                if (!a.empty() && !b.empty()) {
                    uint16_t aa = ParseU16_4(a);
                    uint16_t bb = ParseU16_4(b);
                    if (bb < aa) std::swap(aa, bb);
                    out.push_back({aa, bb});
                }
            } else {
                // single
                uint16_t v = ParseU16_4(seg);
                out.push_back({v, v});
            }
        }
        if (j == std::wstring::npos) break;
        i = j + 1;
    }

    return !out.empty();
}

bool IndexCatalog::LoadFromFile(const std::filesystem::path& path, std::wstring* err) {
    spans.clear();
    labelOrder.clear();

    std::ifstream f(path, std::ios::binary);
    if (!f) { if (err) *err = L"Failed to open TEXT_RSC_indices.txt"; return false; }

    std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::wstring content = winutil::WidenUtf8(bytes);

    std::wstring currentHeader;

    auto pushLabelOrder = [&](const std::wstring& label) {
        for (const auto& s : labelOrder) if (_wcsicmp(s.c_str(), label.c_str()) == 0) return;
        labelOrder.push_back(label);
    };

    size_t pos = 0;
    while (pos < content.size()) {
        size_t eol = content.find(L'\n', pos);
        std::wstring line = (eol == std::wstring::npos) ? content.substr(pos) : content.substr(pos, eol - pos);
        pos = (eol == std::wstring::npos) ? content.size() : (eol + 1);

        line = Trim(line);
        if (line.empty()) continue;

        // Skip obvious non-index scaffolding (wiki chrome)
        if (line.find(L"UESPWiki") != std::wstring::npos) continue;
        if (line.find(L"⧼") != std::wstring::npos) continue;

        if (!StartsWithDigit(line)) {
            // Heading line
            currentHeader = StripParens(line);
            continue;
        }

        std::wstring left = line;
        std::wstring label;

        size_t eq = line.find(L'=');
        if (eq != std::wstring::npos) {
            left = Trim(line.substr(0, eq));
            label = Trim(line.substr(eq + 1));
            label = StripParens(label);
            // strip surrounding quotes
            if (label.size() >= 2 && ((label.front() == L'"' && label.back() == L'"') || (label.front() == L'\'' && label.back() == L'\''))) {
                label = label.substr(1, label.size() - 2);
                label = Trim(label);
            }
        } else {
            label = currentHeader;
        }

        if (label.empty()) label = L"Uncategorized";

        std::vector<std::pair<uint16_t,uint16_t>> parts;
        if (!ParseIndexList(left, parts)) continue;

        pushLabelOrder(label);
        for (auto [a,b] : parts) {
            spans.push_back({a,b,label});
        }
    }

    if (spans.empty()) {
        if (err) *err = L"IndexCatalog loaded but produced 0 spans.";
        return false;
    }
    return true;
}

std::wstring_view IndexCatalog::LabelFor(uint16_t id) const {
    for (const auto& s : spans) {
        if (id >= s.a && id <= s.b) return s.label;
    }
    return {};
}

} // namespace arena2
