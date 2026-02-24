#include "pch.h"
#include "IndicesPrefsWindow.h"
#include "MainWindow.h"
#include "../resource.h"

namespace ui {

static const int COL_MACRO = 0;
static const int COL_DESC  = 1;

const wchar_t* IndicesPrefsWindow::ClassName() { return L"DaggerfallCS_IndicesPrefs"; }

bool IndicesPrefsWindow::Create(HINSTANCE hInst, HWND owner, MainWindow* main) {
    m_owner = owner;
    m_main = main;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hInstance = hInst;
    wc.lpfnWndProc = IndicesPrefsWindow::WndProc;
    wc.lpszClassName = ClassName();
    wc.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassExW(&wc);

    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        ClassName(),
        L"Preferences - Indices",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 980, 560,
        owner, nullptr, hInst, this
    );

    return m_hwnd != nullptr;
}

void IndicesPrefsWindow::Show() {
    if (!m_hwnd) return;
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
}

void IndicesPrefsWindow::Hide() {
    if (!m_hwnd) return;
    ShowWindow(m_hwnd, SW_HIDE);
}

bool IndicesPrefsWindow::OnCreate() {
    m_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 100, 100, m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_INDICES_LIST)), GetModuleHandleW(nullptr), nullptr);
    ListView_SetExtendedListViewStyle(m_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    m_group = CreateWindowExW(0, L"BUTTON", L"Selected Macro",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        0, 0, 100, 100, m_hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

    auto mkStatic = [&](INT_PTR id, const wchar_t* text) {
        return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
            0, 0, 100, 20, m_hwnd, reinterpret_cast<HMENU>(id), GetModuleHandleW(nullptr), nullptr);
    };

    m_macro   = mkStatic(IDC_INDICES_STATIC_MACRO,   L"Macro:");
    m_handler = mkStatic(IDC_INDICES_STATIC_HANDLER, L"Handler:");
    m_type    = mkStatic(IDC_INDICES_STATIC_TYPE,    L"Type:");
    m_impl    = mkStatic(IDC_INDICES_STATIC_IMPL,    L"Implemented:");
    m_comment = mkStatic(IDC_INDICES_STATIC_COMMENT, L"Comment:");

    m_value = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        0, 0, 100, 100, m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_INDICES_VALUE)), GetModuleHandleW(nullptr), nullptr);

    m_apply = CreateWindowExW(0, L"BUTTON", L"Apply",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 100, 28, m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_INDICES_APPLY)), GetModuleHandleW(nullptr), nullptr);

    InitColumns();
    PopulateList();
    if (m_sel >= 0) FillRightPanel();
    return true;
}

void IndicesPrefsWindow::OnDestroy() {
    m_hwnd = nullptr;
}

void IndicesPrefsWindow::OnSize(int cx, int cy) {
    const int pad = 10;
    const int leftW = (int)(cx * 0.55);

    MoveWindow(m_list, pad, pad, leftW - pad * 2, cy - pad * 2, TRUE);

    int rightX = leftW + pad;
    int rightW = cx - rightX - pad;

    MoveWindow(m_group, rightX, pad, rightW, cy - pad * 2, TRUE);

    int x = rightX + 12;
    int w = rightW - 24;
    int y = pad + 24;

    auto placeStatic = [&](HWND h, int hgt) {
        MoveWindow(h, x, y, w, hgt, TRUE);
        y += hgt + 6;
    };

    placeStatic(m_macro, 18);
    placeStatic(m_handler, 18);
    placeStatic(m_type, 18);
    placeStatic(m_impl, 18);
    placeStatic(m_comment, 54);

    int applyH = 28;
    int groupBottom = pad + (cy - pad * 2);

    MoveWindow(m_apply, x, groupBottom - applyH - 10, 120, applyH, TRUE);

    int valueTop = y;
    int valueBottom = groupBottom - applyH - 18;
    int valueH = valueBottom - valueTop;
    if (valueH < 80) valueH = 80;

    MoveWindow(m_value, x, valueTop, w, valueH, TRUE);
}

void IndicesPrefsWindow::InitColumns() {
    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    col.cx = 100;
    col.iSubItem = COL_MACRO;
    col.pszText = const_cast<wchar_t*>(L"Macro");
    ListView_InsertColumn(m_list, COL_MACRO, &col);

    col.cx = 520;
    col.iSubItem = COL_DESC;
    col.pszText = const_cast<wchar_t*>(L"Description");
    ListView_InsertColumn(m_list, COL_DESC, &col);
}

void IndicesPrefsWindow::Refresh() {
    PopulateList();
    FillRightPanel();
}

void IndicesPrefsWindow::PopulateList() {
    if (!m_main) return;

    ListView_DeleteAllItems(m_list);

    const auto& rows = m_main->GetIndicesRows();
    for (int i = 0; i < (int)rows.size(); ++i) {
        const auto& r = rows[i];

        LVITEMW it{};
        it.mask = LVIF_TEXT | LVIF_PARAM;
        it.iItem = i;
        it.pszText = const_cast<wchar_t*>(r.tokenW.c_str());
        it.lParam = (LPARAM)i;
        ListView_InsertItem(m_list, &it);

        ListView_SetItemText(m_list, i, COL_DESC, const_cast<wchar_t*>(r.descW.c_str()));
    }

    if (!rows.empty()) {
        m_sel = 0;
        ListView_SetItemState(m_list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    } else {
        m_sel = -1;
    }
}

void IndicesPrefsWindow::FillRightPanel() {
    if (!m_main || m_sel < 0) return;
    const auto& rows = m_main->GetIndicesRows();
    if (m_sel >= (int)rows.size()) return;

    const auto& r = rows[m_sel];

    auto setStatic = [](HWND h, const std::wstring& label, const std::wstring& value) {
        std::wstring s = label + value;
        SetWindowTextW(h, s.c_str());
    };

    setStatic(m_macro,   L"Macro: ", r.tokenW);
    setStatic(m_handler, L"Handler: ", r.handlerW);
    setStatic(m_type,    L"Type: ", r.typeW);
    setStatic(m_impl,    L"Implemented: ", r.implW);
    setStatic(m_comment, L"Comment: ", r.commentW);

    SetWindowTextW(m_value, r.valueW.c_str());

    EnableWindow(m_value, r.implemented ? TRUE : FALSE);
    EnableWindow(m_apply, r.implemented ? TRUE : FALSE);
}

void IndicesPrefsWindow::OnSelectionChanged(int item) {
    m_sel = item;
    FillRightPanel();
}

void IndicesPrefsWindow::OnApply() {
    if (!m_main || m_sel < 0) return;
    wchar_t buf[8192]{};
    GetWindowTextW(m_value, buf, 8192);
    m_main->SetIndexOverrideByRow(m_sel, buf);
    FillRightPanel();
}

LRESULT CALLBACK IndicesPrefsWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    IndicesPrefsWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<IndicesPrefsWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<IndicesPrefsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_CREATE:
        return self->OnCreate() ? 0 : -1;
    case WM_DESTROY:
        self->OnDestroy();
        return 0;
    case WM_SIZE:
        self->OnSize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_CLOSE:
        self->Hide();
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_INDICES_APPLY) { self->OnApply(); return 0; }
        break;
    case WM_NOTIFY:
        if (((LPNMHDR)lParam)->hwndFrom == self->m_list && ((LPNMHDR)lParam)->code == LVN_ITEMCHANGED) {
            auto* nmlv = (NMLISTVIEW*)lParam;
            if ((nmlv->uChanged & LVIF_STATE) && (nmlv->uNewState & LVIS_SELECTED)) {
                self->OnSelectionChanged(nmlv->iItem);
                return 0;
            }
        }
        break;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace ui