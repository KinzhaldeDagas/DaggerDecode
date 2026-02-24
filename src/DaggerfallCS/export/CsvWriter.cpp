#include "pch.h"
\
#include "CsvWriter.h"

namespace csv {

bool WriteUtf8File(const std::filesystem::path& path, const std::string& data, std::wstring* err) {
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        if (err) *err = L"Failed to open output file.";
        return false;
    }
    // UTF-8 BOM-free
    f.write(data.data(), (std::streamsize)data.size());
    if (!f.good()) {
        if (err) *err = L"Failed to write output file.";
        return false;
    }
    return true;
}

std::string EscapeCell(std::string_view s) {
    bool needQuotes = false;
    for (char c : s) {
        if (c == '"' || c == ',' || c == '\n' || c == '\r') { needQuotes = true; break; }
    }
    if (!needQuotes) return std::string(s);

    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s) {
        if (c == '"') out.push_back('"');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

void AppendRow(std::string& out, const std::vector<std::string>& cells) {
    for (size_t i = 0; i < cells.size(); ++i) {
        if (i) out.push_back(',');
        auto esc = EscapeCell(cells[i]);
        out.append(esc);
    }
    out.append("\r\n");
}

}