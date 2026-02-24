#pragma once
#include "../pch.h"

namespace csv {

bool WriteUtf8File(const std::filesystem::path& path, const std::string& data, std::wstring* err);
std::string EscapeCell(std::string_view s);
void AppendRow(std::string& out, const std::vector<std::string>& cells);

}
