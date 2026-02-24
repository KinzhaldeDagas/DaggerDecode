#pragma once
#include "../pch.h"

namespace ui {

class MainWindow;

class IndicesPrefsWindow {
public:
    bool Create(HINSTANCE hInst, HWND owner, MainWindow* main);
    void Show();
    void Hide();
    bool IsOpen() const { return m_hwnd != nullptr; }

    void Refresh();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hwnd{};
    HWND m_owner{};
    HWND m_list{};

    HWND m_group{};
    HWND m_value{};
    HWND m_apply{};

    HWND m_macro{};
    HWND m_handler{};
    HWND m_type{};
    HWND m_impl{};
    HWND m_comment{};

    int  m_sel{-1};
    MainWindow* m_main{};

    bool OnCreate();
    void OnDestroy();
    void OnSize(int cx, int cy);
    void OnSelectionChanged(int item);
    void OnApply();

    void InitColumns();
    void PopulateList();
    void FillRightPanel();

    static const wchar_t* ClassName();
};

} // namespace ui
