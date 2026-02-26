#pragma once
#include "../pch.h"

namespace winutil {

std::optional<std::filesystem::path> PickFolder(HWND owner, const wchar_t* title);
std::optional<std::filesystem::path> PickFile(HWND owner, const wchar_t* title, const wchar_t* filterSpec = nullptr);
std::filesystem::path GetExeDirectory();
void SetStatusText(HWND hStatus, const std::wstring& s);

std::wstring WidenUtf8(std::string_view s);
std::string NarrowUtf8(std::wstring_view s);

}
