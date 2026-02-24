#include "pch.h"
#include "WinUtil.h"
#include <shobjidl.h>

namespace winutil {

std::optional<std::filesystem::path> PickFolder(HWND owner, const wchar_t* title) {
    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    if (FAILED(hr) || !pfd) return std::nullopt;

    DWORD opts = 0;
    pfd->GetOptions(&opts);
    pfd->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    pfd->SetTitle(title);

    hr = pfd->Show(owner);
    if (FAILED(hr)) { pfd->Release(); return std::nullopt; }

    IShellItem* psi = nullptr;
    hr = pfd->GetResult(&psi);
    if (FAILED(hr) || !psi) { pfd->Release(); return std::nullopt; }

    PWSTR psz = nullptr;
    hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &psz);

    std::optional<std::filesystem::path> out;
    if (SUCCEEDED(hr) && psz) {
        out = std::filesystem::path(psz);
        CoTaskMemFree(psz);
    }

    psi->Release();
    pfd->Release();
    return out;
}


std::filesystem::path GetExeDirectory() {
    wchar_t buf[MAX_PATH]{};
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {};
    std::filesystem::path p(buf);
    return p.has_parent_path() ? p.parent_path() : std::filesystem::path{};
}

void SetStatusText(HWND hStatus, const std::wstring& s) {
    if (!hStatus) return;
    SendMessageW(hStatus, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(s.c_str()));
}

std::wstring WidenUtf8(std::string_view s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (len <= 0) return {};
    std::wstring out;
    out.resize((size_t)len);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), len);
    return out;
}

std::string NarrowUtf8(std::wstring_view s) {
    if (s.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out;
    out.resize((size_t)len);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), len, nullptr, nullptr);
    return out;
}

}
