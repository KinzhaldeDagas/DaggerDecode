#include "pch.h"
#include "ui/MainWindow.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    (void)hr;

    ui::MainWindow win;
    if (!win.Create(hInst)) {
        MessageBoxW(nullptr, L"Failed to create window.", L"Fatal", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    ShowWindow(win.Hwnd(), nCmdShow);
    UpdateWindow(win.Hwnd());

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}
