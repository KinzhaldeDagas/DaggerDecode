#include "pch.h"
#include "MainWindow.h"
#include "../resource.h"
#include "../util/WinUtil.h"
#include "../export/CsvWriter.h"
#include "../arena2/QuestOpcodeDisasm.h"
#include "../battlespire/BattlespireFormats.h"
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

namespace ui {

struct MainWindow::LoadResult {
    bool ok{ false };
    std::wstring err;
    arena2::TextRsc text;
    arena2::QuestCatalog quests;
    bool questsOk{ false };
    std::vector<battlespire::BsaArchive> bsaArchives;
    bool bsaOk{ false };
};

static const wchar_t* kWndClass = L"DaggerfallCS_MainWindow";
static std::wstring HexU32(uint32_t v);

static std::wstring ToLowerCopy(std::wstring s) {
    for (auto& c : s) c = (wchar_t)towlower(c);
    return s;
}

static constexpr UINT IDM_BSA_DIALOGUE_SPEAK = 42001;
static constexpr UINT IDM_BSA_ENTRY_SPEAK = 42002;


struct DialogueCodeParts {
    std::wstring speaker;
    int terminalNumber{ -1 };
};

static bool TryParseDialogueCodeParts(const std::wstring& rawCode, DialogueCodeParts& out) {
    // Dialogue cells can include punctuation or full clip-like forms.
    // Accept codes embedded anywhere like: DK92, DKTALK92, DK-WAIT-92.
    std::wstring code = rawCode;
    for (auto& c : code) c = (wchar_t)towupper(c);

    std::wsmatch m;
    static const std::wregex re(LR"(([A-Z]{2})(?:[^A-Z0-9]*(?:TALK|WAIT))?[^0-9]*(\d{1,3}))");
    if (!std::regex_search(code, m, re)) return false;

    out.speaker = m[1].str();
    out.terminalNumber = _wtoi(m[2].str().c_str());
    return out.terminalNumber >= 0;
}

static bool TryParseFlcVariant(const std::wstring& name, std::wstring& speaker, std::wstring& mode, int& variant) {
    size_t slash = name.find_last_of(L"/\\");
    std::wstring bn = (slash == std::wstring::npos) ? name : name.substr(slash + 1);
    for (auto& c : bn) c = (wchar_t)towlower(c);

    std::wsmatch m;
    static const std::wregex re(LR"(^([a-z]{2})(talk|wait)(\d{2})\.flc$)");
    if (!std::regex_match(bn, m, re)) return false;

    speaker = m[1].str();
    mode = m[2].str();
    for (auto& c : speaker) c = (wchar_t)towupper(c);
    for (auto& c : mode) c = (wchar_t)towupper(c);
    variant = _wtoi(m[3].str().c_str());
    return true;
}



static std::wstring TopicGroupForLabel(const std::wstring& label) {
    std::wstring s = ToLowerCopy(label);

    auto has = [&](const wchar_t* needle) { return s.find(needle) != std::wstring::npos; };

    if (has(L"dialogue") || has(L"greeting") || has(L"rumor") || has(L"news") || has(L"where is") ||
        has(L"talk") || has(L"inquiry") || has(L"court") || has(L"guard") || has(L"tavern") || has(L"bank") ||
        has(L"shop") || has(L"merchant") || has(L"thieves") || has(L"brotherhood") || has(L"mages") ||
        has(L"temple") || has(L"knight") || has(L"order")) {
        return L"Dialogue";
    }

    if (has(L"message") || has(L"confirm") || has(L"help") || has(L"hint") || has(L"cannot") || has(L"no ") ||
        has(L"reserved") || has(L"dummy") || has(L"void") || has(L"insert action") || has(L"bad text") ||
        has(L"training") || has(L"rest") || has(L"loiter") || has(L"fast travel") || has(L"reputation") ||
        has(L"vamp") || has(L"disease") || has(L"poison") || has(L"repair")) {
        return L"UI Messages";
    }

    if (has(L"template") || has(L"nouns") || has(L"adjective") || has(L"colors") || has(L"phrases") ||
        has(L"names") || has(L"biograph") || has(L"god") || has(L"class") || has(L"skills") ||
        has(L"spell") || has(L"effect") || has(L"artifact") || has(L"item info")) {
        return L"Data Tables";
    }

    return L"Other";
}

static std::string CheapPreview(const std::vector<uint8_t>& raw) {
    std::string out;
    out.reserve(220);
    for (size_t i = 0; i < raw.size() && out.size() < 220; ++i) {
        uint8_t b = raw[i];
        if (b >= 0x20 && b <= 0x7E) out.push_back((char)b);
        else if (b == 0x00) out.push_back(' ');
        else if (b == 0xF6) { out.append(" <page> "); }
        else if (b == 0xF9) { out.append(" <font> "); }
        else if (b == 0xFA) { out.append(" <color> "); }
        else if (b == 0xFF) { out.append(" <alt> "); }
        else if (b == 0xFE) { break; }
        else { out.push_back(' '); }
    }
    return out;
}

static std::wstring FormatRecLabel(uint16_t id) {
    wchar_t buf[64]{};
    swprintf_s(buf, L"0x%04X (%u)", (unsigned)id, (unsigned)id);
    return buf;
}

static bool IsBookLikeLabel(const std::wstring& label) {
    std::wstring s = ToLowerCopy(label);
    auto has = [&](const wchar_t* needle) { return s.find(needle) != std::wstring::npos; };
    return has(L"book") || has(L"notebook") || has(L"journal") || has(L"diary") || has(L"spellbook") ||
           has(L"guide") || has(L"chronicle") || has(L"history") || has(L"biography") || has(L"biograph");
}

static bool LooksLikeBookRecord(arena2::TextRecord& rec, const std::vector<uint8_t>& fileBytes, std::wstring_view label) {
    rec.EnsureParsed(fileBytes);
    if (rec.subrecords.empty()) return false;

    int score = 0;
    if (IsBookLikeLabel(std::wstring(label))) score += 2;

    size_t plainBytes = 0;
    for (auto& sr : rec.subrecords) {
        auto& tok = sr.EnsureTokens();
        plainBytes += tok.plain.size();
        if (tok.hasEndOfPage) score += 3;
        if (tok.hasFontScript) score += 2;
        for (const auto& t : tok.tokens) {
            if (t.type == arena2::TokenType::BookImage) {
                score += 2;
                break;
            }
        }
    }

    if (rec.subrecords.size() >= 2) score += 1;
    if (plainBytes >= 350) score += 1;

    return score >= 3;
}

static std::wstring CompactLine(std::wstring s) {
    for (auto& ch : s) {
        if (ch == L'\r' || ch == L'\n' || ch == L'\t') ch = L' ';
    }

    std::wstring out;
    out.reserve(s.size());
    bool prevSpace = true;
    for (wchar_t ch : s) {
        if (ch < 32) continue;
        bool isSpace = (ch == L' ');
        if (isSpace && prevSpace) continue;
        out.push_back(ch);
        prevSpace = isSpace;
    }

    while (!out.empty() && out.front() == L' ') out.erase(out.begin());
    while (!out.empty() && out.back() == L' ') out.pop_back();
    return out;
}


static bool IsLikelyUtf8Printable(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) return true;
    size_t printable = 0;
    size_t control = 0;
    size_t sample = std::min<size_t>(bytes.size(), 4096);
    for (size_t i = 0; i < sample; ++i) {
        uint8_t b = bytes[i];
        if (b == 9 || b == 10 || b == 13) { printable++; continue; }
        if (b >= 32 && b < 127) printable++;
        else if (b < 32) control++;
        else printable++;
    }
    return control * 8 < sample;
}

static std::wstring HexDumpPreview(const std::vector<uint8_t>& bytes, size_t maxBytes = 256) {
    std::wstring out;
    size_t n = std::min(bytes.size(), maxBytes);
    wchar_t line[128]{};
    for (size_t i = 0; i < n; i += 16) {
        swprintf_s(line, L"%08X: ", (unsigned)i);
        out += line;
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < n) {
                swprintf_s(line, L"%02X ", (unsigned)bytes[i + j]);
                out += line;
            } else out += L"   ";
        }
        out += L" |";
        for (size_t j = 0; j < 16 && i + j < n; ++j) {
            wchar_t c = (bytes[i + j] >= 32 && bytes[i + j] < 127) ? (wchar_t)bytes[i + j] : L'.';
            out.push_back(c);
        }
        out += L"|\r\n";
    }
    if (bytes.size() > n) out += L"...\r\n";
    return out;
}


static std::wstring TrimWs(std::wstring v) {
    auto issp = [](wchar_t c) { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n'; };
    while (!v.empty() && issp(v.front())) v.erase(v.begin());
    while (!v.empty() && issp(v.back())) v.pop_back();
    return v;
}

static std::wstring ToLowerWs(std::wstring v) {
    for (auto& c : v) c = (wchar_t)towlower(c);
    return v;
}

static bool EndsWithWs(const std::wstring& s, const wchar_t* suffix) {
    size_t n = wcslen(suffix);
    if (s.size() < n) return false;
    return _wcsicmp(s.c_str() + (s.size() - n), suffix) == 0;
}

static std::vector<std::vector<std::wstring>> ParseTabLines(const std::wstring& text) {
    std::vector<std::vector<std::wstring>> rows;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find(L'\n', start);
        std::wstring line = (end == std::wstring::npos) ? text.substr(start) : text.substr(start, end - start);
        if (!line.empty() && line.back() == L'\r') line.pop_back();

        std::vector<std::wstring> cols;
        size_t cstart = 0;
        while (cstart <= line.size()) {
            size_t cend = line.find(L'\t', cstart);
            std::wstring cell = (cend == std::wstring::npos) ? line.substr(cstart) : line.substr(cstart, cend - cstart);
            cols.push_back(TrimWs(std::move(cell)));
            if (cend == std::wstring::npos) break;
            cstart = cend + 1;
        }

        bool any = false;
        for (const auto& c : cols) {
            if (!c.empty()) { any = true; break; }
        }
        if (any) rows.push_back(std::move(cols));

        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return rows;
}

static std::wstring BaseNameOnly(const std::wstring& name) {
    size_t slash = name.find_last_of(L"/\\");
    std::wstring leaf = (slash == std::wstring::npos) ? name : name.substr(slash + 1);
    return ToLowerWs(leaf);
}

static std::wstring ClassifyTxtBsaEntryCategory(const std::string& entryNameUtf8) {
    std::wstring name = BaseNameOnly(winutil::WidenUtf8(entryNameUtf8));

    std::wregex reDialogue(LR"(^[a-z]{2}\d+[btmf]\.txt$)");
    std::wregex reBook(LR"(^bk\d+_\d+\.txt$)");
    std::wregex reAmtbl(LR"(^amtbl\d+\.txt$)");
    std::wregex reLShard(LR"(^l\d+\.txt$)");
    std::wregex reBugs(LR"(^bugs(l\d+|mult)?\.txt$)");

    if (std::regex_match(name, reDialogue)) return L"Dialogue Bundles";
    if (std::regex_match(name, reBook) || name == L"book0000.txt") return L"Books";

    if (name == L"iteml2.txt" || name == L"siteml2.txt" || name == L"magic.txt" ||
        name == L"mg0_gen.txt" || name == L"mg2_spc.txt" || name == L"sigils.txt") {
        return L"Item/Magic/Sigil Tables";
    }

    if (std::regex_match(name, reAmtbl) || std::regex_match(name, reLShard)) return L"Auxiliary Tables/Lists";
    if (std::regex_match(name, reBugs) || name == L"big_bugs.txt" || name == L"lil_bugs.txt") return L"Bug Notes";

    if (name == L"adr.txt" || name == L"comlog.txt" || name == L"errorlog.txt" ||
        name == L"julian.txt" || name == L"julian!!.txt" || name == L"memlog.txt" ||
        name == L"note.txt" || name == L"spire.txt" || name == L"stuff.txt" ||
        name == L"temp.txt" || name == L"tmp.txt" || name == L"todo.txt") {
        return L"Logs/Scratch";
    }

    if (EndsWithWs(name, L".txt")) return L"Other TXT";
    return L"Non-TXT";
}

static int DialogueVariantRank(wchar_t v) {
    switch ((wchar_t)towlower(v)) {
    case L'b': return 0;
    case L't': return 1;
    case L'm': return 2;
    case L'f': return 3;
    default: return 99;
    }
}

static bool TryParseDialogueBundleName(const std::wstring& name, std::wstring& questCode, int& level, wchar_t& variant) {
    static const std::wregex reDialogueParts(LR"(^([a-z]{2})(\d+)([btmf])\.txt$)");
    std::wsmatch m;
    if (!std::regex_match(name, m, reDialogueParts)) return false;

    questCode = m[1].str();
    level = _wtoi(m[2].str().c_str());
    variant = m[3].str().empty() ? L'?' : m[3].str()[0];
    return true;
}

static void SortBsaCategoryEntries(const battlespire::BsaArchive& a, const std::wstring& cat, std::vector<size_t>& entries) {
    if (entries.size() < 2) return;

    if (_wcsicmp(cat.c_str(), L"Dialogue Bundles") == 0) {
        std::stable_sort(entries.begin(), entries.end(), [&](size_t lhs, size_t rhs) {
            const std::wstring ln = BaseNameOnly(winutil::WidenUtf8(a.entries[lhs].name));
            const std::wstring rn = BaseNameOnly(winutil::WidenUtf8(a.entries[rhs].name));

            std::wstring lq, rq;
            int ll = 0, rl = 0;
            wchar_t lv = 0, rv = 0;
            const bool lok = TryParseDialogueBundleName(ln, lq, ll, lv);
            const bool rok = TryParseDialogueBundleName(rn, rq, rl, rv);
            if (lok && rok) {
                int qcmp = _wcsicmp(lq.c_str(), rq.c_str());
                if (qcmp != 0) return qcmp < 0;   // quest/group
                if (ll != rl) return ll < rl;      // level/index
                if (lv != rv) return DialogueVariantRank(lv) < DialogueVariantRank(rv); // variant (b/t/m/f)
                return ln < rn;
            }
            if (lok != rok) return lok; // parsed dialogue bundles first
            return ln < rn;
        });
        return;
    }

    std::stable_sort(entries.begin(), entries.end(), [&](size_t lhs, size_t rhs) {
        std::wstring ln = BaseNameOnly(winutil::WidenUtf8(a.entries[lhs].name));
        std::wstring rn = BaseNameOnly(winutil::WidenUtf8(a.entries[rhs].name));
        return ln < rn;
    });
}

static bool IsDialogueScriptHeaderCell(const std::wstring& cell) {
    std::wstring c = ToLowerWs(TrimWs(cell));
    return c == L"saycode" || c == L"npc say" || c == L"replycode" || c == L"pc reply" || c == L"do this action" || c == L"goto";
}

static bool IsRoutingHeaderCell(const std::wstring& cell) {
    std::wstring c = ToLowerWs(TrimWs(cell));
    return c == L"talkgroup" || c == L"notes";
}

static bool IsRoutingKeywordCell(const std::wstring& cell) {
    std::wstring c = ToLowerWs(TrimWs(cell));
    return c == L"talkcheck" || c == L"talktable" || c == L"random" || c == L"first" || c == L"second" || c == L"third" || c.find(L"fourth") != std::wstring::npos;
}

static size_t CountNonEmptyCells(const std::vector<std::wstring>& row) {
    size_t n = 0;
    for (const auto& c : row) {
        if (!TrimWs(c).empty()) n++;
    }
    return n;
}

static std::wstring DeriveBookTitle(arena2::TextRecord& rec, const std::vector<uint8_t>& fileBytes, std::wstring_view indexLabel) {
    std::wstring label(indexLabel);
    std::wstring labelLow = ToLowerCopy(label);
    const bool looksDialogueLabel = (labelLow.find(L"dialogue") != std::wstring::npos || labelLow.find(L"message") != std::wstring::npos);
    if (!label.empty() && _wcsicmp(label.c_str(), L"Uncategorized") != 0 && !looksDialogueLabel) {
        std::wstring sline = CompactLine(label);
        if (!sline.empty()) return sline;
    }

    rec.EnsureParsed(fileBytes);
    if (!rec.subrecords.empty()) {
        auto& tok = rec.subrecords[0].EnsureTokens();
        std::wstring plain = winutil::WidenUtf8(tok.plain);

        size_t pos = 0;
        while (pos < plain.size()) {
            size_t eol = plain.find(L'\n', pos);
            std::wstring line = (eol == std::wstring::npos) ? plain.substr(pos) : plain.substr(pos, eol - pos);
            line = CompactLine(line);
            if (!line.empty() && line[0] != L'%' && line[0] != L'_') {
                if (line.size() > 72) line = line.substr(0, 72) + L"...";
                return line;
            }
            if (eol == std::wstring::npos) break;
            pos = eol + 1;
        }
    }

    wchar_t buf[64]{};
    swprintf_s(buf, L"Book 0x%04X", (unsigned)rec.recordId);
    return buf;
}

static std::wstring FormatBookRecLabel(uint16_t id, std::wstring_view title) {
    std::wstring t(title);
    if (t.empty()) t = L"Book";
    wchar_t buf[96]{};
    swprintf_s(buf, L"[0x%04X]", (unsigned)id);
    t.append(L" ");
    t.append(buf);
    return t;
}

static void InitListColumns(HWND hList) {
    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.cx = 80;
    col.pszText = const_cast<wchar_t*>(L"Index");
    ListView_InsertColumn(hList, 0, &col);

    col.cx = 600;
    col.iSubItem = 1;
    col.pszText = const_cast<wchar_t*>(L"Preview");
    ListView_InsertColumn(hList, 1, &col);
}

bool MainWindow::Create(HINSTANCE hInst) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hInstance = hInst;
    wc.lpfnWndProc = MainWindow::WndProc;
    wc.lpszClassName = kWndClass;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    if (!RegisterClassExW(&wc)) return false;

    m_hwnd = CreateWindowExW(
        0, kWndClass, L"Daggerfall-CS (TEXT.RSC MVP)",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 760,
        nullptr, nullptr, hInst, this
    );
    return m_hwnd != nullptr;
}

static HMENU BuildMenuBar() {
    HMENU hMenubar = CreateMenu();
    HMENU hFile = CreateMenu();
    HMENU hExport = CreateMenu();
    HMENU hHelp = CreateMenu();

    AppendMenuW(hFile, MF_STRING, IDM_FILE_OPEN_ARENA2, L"Open ARENA2 Folder...");
    AppendMenuW(hFile, MF_STRING, IDM_FILE_OPEN_SPIRE, L"Open spire Folder...");
    AppendMenuW(hFile, MF_STRING, IDM_FILE_EXTRACT_BSA, L"Extract BSA...");
    AppendMenuW(hFile, MF_STRING, IDM_FILE_INDICES, L"Indices...");
    AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFile, MF_STRING, IDM_FILE_EXIT, L"Exit");

    AppendMenuW(hExport, MF_STRING, IDM_EXPORT_SUBRECORDS, L"Export TEXT_RSC_Subrecords.csv...");
    AppendMenuW(hExport, MF_STRING, IDM_EXPORT_TOKENS, L"Export TEXT_RSC_Tokens.csv...");
    AppendMenuW(hExport, MF_STRING, IDM_EXPORT_VARIABLES, L"Export TEXT_RSC_Variables.csv...");
    AppendMenuW(hExport, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hExport, MF_STRING, IDM_EXPORT_QUESTS, L"Export QUESTS_List.csv...");
    AppendMenuW(hExport, MF_STRING, IDM_EXPORT_QUEST_STAGES, L"Export QUESTS_Stages.csv...");
    AppendMenuW(hExport, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hExport, MF_STRING, IDM_EXPORT_TES4_QD, L"Export TES4_QuestDialogue.txt...");

    AppendMenuW(hHelp, MF_STRING, IDM_HELP_ABOUT, L"About");

    AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hFile, L"File");
    AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hExport, L"Export");
    AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hHelp, L"Help");
    return hMenubar;
}

bool MainWindow::OnCreate() {
    SetMenu(m_hwnd, BuildMenuBar());

    m_tree = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
        0, 0, 100, 100, m_hwnd, (HMENU)(INT_PTR)IDC_MAIN_TREE, GetModuleHandleW(nullptr), nullptr);

    m_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 100, 100, m_hwnd, (HMENU)(INT_PTR)IDC_SUBRECORD_LIST, GetModuleHandleW(nullptr), nullptr);
    ListView_SetExtendedListViewStyle(m_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    InitListColumns(m_list);

    m_tabs = CreateWindowExW(0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_CLIPSIBLINGS | WS_TABSTOP,
        0, 0, 100, 24, m_hwnd, (HMENU)(INT_PTR)IDC_QUEST_TABS, GetModuleHandleW(nullptr), nullptr);

    TCITEMW ti{};
    ti.mask = TCIF_TEXT;
    ti.pszText = const_cast<wchar_t*>(L"Data");
    TabCtrl_InsertItem(m_tabs, 0, &ti);
    ti.pszText = const_cast<wchar_t*>(L"Stages");
    TabCtrl_InsertItem(m_tabs, 1, &ti);
    ti.pszText = const_cast<wchar_t*>(L"Text");
    TabCtrl_InsertItem(m_tabs, 2, &ti);

    ShowWindow(m_tabs, SW_HIDE);

    m_preview = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
        0, 0, 100, 100, m_hwnd, (HMENU)(INT_PTR)IDC_PREVIEW_EDIT, GetModuleHandleW(nullptr), nullptr);

    m_status = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, m_hwnd, (HMENU)(INT_PTR)IDC_STATUS_BAR, GetModuleHandleW(nullptr), nullptr);

    m_splitLR.Attach(m_hwnd, m_tree, m_list);
    m_splitLR.SetRatio(0.28);

    // Disable exports until load
    HMENU hMenu = GetMenu(m_hwnd);
    EnableMenuItem(hMenu, IDM_EXPORT_SUBRECORDS, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hMenu, IDM_EXPORT_TOKENS, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hMenu, IDM_EXPORT_VARIABLES, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hMenu, IDM_EXPORT_QUESTS, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hMenu, IDM_EXPORT_QUEST_STAGES, MF_BYCOMMAND | MF_GRAYED);
    DrawMenuBar(m_hwnd);

    InitIndicesModel();
    LoadIndicesOverrides();

    SetStatus(L"Ready. File > Open ARENA2 Folder...");
    return true;
}

void MainWindow::OnDestroy() {
    EndListPreviewEdit(true);
    SaveIndicesOverrides();
    PostQuitMessage(0);
}

void MainWindow::OnSize(int cx, int cy) {
    if (!m_status) return;

    SendMessageW(m_status, WM_SIZE, 0, 0);
    RECT rcStatus{};
    GetWindowRect(m_status, &rcStatus);
    int statusH = rcStatus.bottom - rcStatus.top;

    int clientH = cy - statusH;
    if (clientH < 0) clientH = 0;

    MoveWindow(m_tree, 0, 0, 100, clientH, TRUE);
    MoveWindow(m_list, 0, 0, 100, clientH, TRUE);

    m_splitLR.OnSize(cx, clientH);

    RECT rcTree{};
    GetWindowRect(m_tree, &rcTree);
    int treeW = rcTree.right - rcTree.left;

    int rightX = treeW + 6;
    int rightW = cx - rightX;
    if (rightW < 0) rightW = 0;

    SetWindowPos(m_list, nullptr, rightX, 0, rightW, clientH, SWP_NOZORDER);
    SetWindowPos(m_preview, nullptr, rightX, 0, rightW, clientH, SWP_NOZORDER);

    const bool tabsVisible = (m_tabs && IsWindowVisible(m_tabs));
    const int tabH = tabsVisible ? 26 : 0;
    if (tabsVisible) SetWindowPos(m_tabs, nullptr, rightX, 0, rightW, tabH, SWP_NOZORDER);

    int topH = (int)(clientH * 0.55);
    if (topH < 80) topH = 80;
    if (topH > clientH - 80) topH = clientH - 80;
    SetWindowPos(m_list, nullptr, rightX, tabH ? (tabH + 2) : 0, rightW, topH - (tabH ? (tabH + 2) : 0), SWP_NOZORDER);
    SetWindowPos(m_preview, nullptr, rightX, topH + 6, rightW, clientH - (topH + 6), SWP_NOZORDER);
}

void MainWindow::OnCommand(WORD id) {
    switch (id) {
    case IDM_FILE_OPEN_ARENA2: CmdOpenArena2(); break;
    case IDM_FILE_OPEN_SPIRE: CmdOpenSpire(); break;
    case IDM_FILE_EXTRACT_BSA: CmdExtractBsa(); break;
    case IDM_FILE_INDICES:
        if (!m_indicesWnd.IsOpen()) m_indicesWnd.Create(GetModuleHandleW(nullptr), m_hwnd, this);
        m_indicesWnd.Refresh();
        m_indicesWnd.Show();
        break;
    case IDM_FILE_EXIT: DestroyWindow(m_hwnd); break;
    case IDM_EXPORT_SUBRECORDS: CmdExportSubrecords(); break;
    case IDM_EXPORT_TOKENS: CmdExportTokens(); break;
    case IDM_EXPORT_VARIABLES: CmdExportVariables(); break;
    case IDM_EXPORT_QUESTS: CmdExportQuests(); break;
    case IDM_EXPORT_QUEST_STAGES: CmdExportQuestStages(); break;
    case IDM_EXPORT_TES4_QD: CmdExportTes4QuestDialogue(); break;
    case IDM_BSA_DIALOGUE_SPEAK: break;
    case IDM_HELP_ABOUT:
        MessageBoxW(m_hwnd, L"Daggerfall-CS MVP\n\nLoads TEXT.RSC and Battlespire BSA resources and exports CSV.", L"About", MB_OK | MB_ICONINFORMATION);
        break;
    default: break;
    }
}

void MainWindow::SetStatus(const std::wstring& s) {
    winutil::SetStatusText(m_status, s);
}

void MainWindow::PopulateTree() {
    StartTreeBuild();
}

void MainWindow::StartTreeBuild() {
    KillTimer(m_hwnd, TIMER_POP_TREE);

    TreeView_DeleteAllItems(m_tree);
    m_pendingTreeIds.clear();
    m_pendingTreeParents.clear();
    m_treeInsertPos = 0;
    m_treeRootText = nullptr;
    m_treeRootQuests = nullptr;
    m_treeRootBsa = nullptr;
    m_treePayloads.clear();
    m_pendingQuestIdx.clear();
    m_pendingQuestParents.clear();
    m_questInsertPos = 0;
    m_bookRecordIds.clear();
    m_bookRecordTitles.clear();

    if (!m_varHashLoaded) {
        std::filesystem::path exeDir = winutil::GetExeDirectory();
        std::filesystem::path p1 = exeDir / L"TEXT_VARIABLE_HASHES.txt";
        std::filesystem::path p2 = exeDir / L"data" / L"TEXT_VARIABLE_HASHES.txt";
        std::wstring err;
        if (std::filesystem::exists(p1)) m_varHashLoaded = m_varHash.LoadFromFile(p1, &err);
        else if (std::filesystem::exists(p2)) m_varHashLoaded = m_varHash.LoadFromFile(p2, &err);
        else m_varHashLoaded = false;
    }

    if (!m_indexLoaded) {
        std::filesystem::path exeDir = winutil::GetExeDirectory();
        std::filesystem::path p1 = exeDir / L"TEXT_RSC_indices.txt";
        std::filesystem::path p2 = exeDir / L"data" / L"TEXT_RSC_indices.txt";

        std::wstring err;
        if (std::filesystem::exists(p1)) m_indexLoaded = m_index.LoadFromFile(p1, &err);
        else if (std::filesystem::exists(p2)) m_indexLoaded = m_index.LoadFromFile(p2, &err);
        else m_indexLoaded = false;
    }

    TVINSERTSTRUCTW ins{};
    ins.hParent = TVI_ROOT;
    ins.hInsertAfter = TVI_LAST;
    ins.item.mask = TVIF_TEXT;
    ins.item.pszText = const_cast<wchar_t*>(L"TEXT.RSC");
    m_treeRootText = (HTREEITEM)SendMessageW(m_tree, TVM_INSERTITEMW, 0, (LPARAM)&ins);

    // Quests root
    ins.item.pszText = const_cast<wchar_t*>(L"QUESTS");
    m_treeRootQuests = (HTREEITEM)SendMessageW(m_tree, TVM_INSERTITEMW, 0, (LPARAM)&ins);
    if (m_treeRootQuests) {
        auto* pr = AddPayload(TreePayload::Kind::QuestsRoot);
        TVITEMW ti{};
        ti.mask = TVIF_PARAM | TVIF_HANDLE;
        ti.hItem = m_treeRootQuests;
        ti.lParam = (LPARAM)pr;
        TreeView_SetItem(m_tree, &ti);
    }

    // BSA root (Battlespire)
    ins.item.pszText = const_cast<wchar_t*>(L"BSA Archives");
    m_treeRootBsa = (HTREEITEM)SendMessageW(m_tree, TVM_INSERTITEMW, 0, (LPARAM)&ins);
    if (m_treeRootBsa) {
        auto* pr = AddPayload(TreePayload::Kind::BsaRoot);
        TVITEMW ti{};
        ti.mask = TVIF_PARAM | TVIF_HANDLE;
        ti.hItem = m_treeRootBsa;
        ti.lParam = (LPARAM)pr;
        TreeView_SetItem(m_tree, &ti);

        for (size_t ai = 0; ai < m_bsaArchives.size(); ++ai) {
            const auto& a = m_bsaArchives[ai];
            std::wstring aname = a.sourcePath.filename().wstring();
            aname += L" (" + std::to_wstring(a.entries.size()) + L" entries)";

            TVINSERTSTRUCTW ains{};
            ains.hParent = m_treeRootBsa;
            ains.hInsertAfter = TVI_LAST;
            ains.item.mask = TVIF_TEXT | TVIF_PARAM;
            ains.item.pszText = const_cast<wchar_t*>(aname.c_str());
            ains.item.lParam = (LPARAM)AddPayload(TreePayload::Kind::BsaArchive, 0, (size_t)-1, ai, (size_t)-1);
            HTREEITEM ah = (HTREEITEM)SendMessageW(m_tree, TVM_INSERTITEMW, 0, (LPARAM)&ains);

            std::map<std::wstring, std::vector<size_t>> txtCategories;
            std::vector<size_t> nonTxt;
            for (size_t ei = 0; ei < a.entries.size(); ++ei) {
                const auto& e = a.entries[ei];
                std::wstring cat = ClassifyTxtBsaEntryCategory(e.name);
                if (cat == L"Non-TXT") nonTxt.push_back(ei);
                else txtCategories[cat].push_back(ei);
            }

            auto insertEntryNode = [&](HTREEITEM parent, size_t ei) {
                const auto& e = a.entries[ei];
                std::wstring label = winutil::WidenUtf8(e.name);
                wchar_t suffix[96]{};
                swprintf_s(suffix, L"  [off=0x%X, size=%u%s]", (unsigned)e.offset, (unsigned)e.packedSize, e.compressionFlag ? L", cmp" : L"");
                label += suffix;

                TVINSERTSTRUCTW eins{};
                eins.hParent = parent;
                eins.hInsertAfter = TVI_LAST;
                eins.item.mask = TVIF_TEXT | TVIF_PARAM;
                eins.item.pszText = const_cast<wchar_t*>(label.c_str());
                eins.item.lParam = (LPARAM)AddPayload(TreePayload::Kind::BsaEntry, 0, (size_t)-1, ai, ei);
                SendMessageW(m_tree, TVM_INSERTITEMW, 0, (LPARAM)&eins);
            };

            const std::vector<std::wstring> catOrder = {
                L"Dialogue Bundles",
                L"Books",
                L"Item/Magic/Sigil Tables",
                L"Auxiliary Tables/Lists",
                L"Bug Notes",
                L"Logs/Scratch",
                L"Other TXT"
            };

            const bool isFlcBsa = (_wcsicmp(a.sourcePath.filename().wstring().c_str(), L"FLC.BSA") == 0);
            if (isFlcBsa) {
                std::vector<size_t> dialogueFlc;
                std::vector<size_t> otherFlc;
                for (size_t ei = 0; ei < a.entries.size(); ++ei) {
                    std::wstring bn = BaseNameOnly(winutil::WidenUtf8(a.entries[ei].name));
                    if (std::regex_match(bn, std::wregex(LR"(^[a-z]{2}\d{2}\.(voc|wav)$)"))) dialogueFlc.push_back(ei);
                    else otherFlc.push_back(ei);
                }

                auto byName = [&](std::vector<size_t>& v) {
                    std::stable_sort(v.begin(), v.end(), [&](size_t lhs, size_t rhs) {
                        std::wstring ln = BaseNameOnly(winutil::WidenUtf8(a.entries[lhs].name));
                        std::wstring rn = BaseNameOnly(winutil::WidenUtf8(a.entries[rhs].name));
                        return ln < rn;
                    });
                };
                byName(dialogueFlc);
                byName(otherFlc);

                auto insCat = [&](const std::wstring& label, const std::vector<size_t>& items) {
                    if (items.empty()) return;
                    std::wstring cLabel = label + L" (" + std::to_wstring(items.size()) + L")";
                    TVINSERTSTRUCTW cins{};
                    cins.hParent = ah;
                    cins.hInsertAfter = TVI_LAST;
                    cins.item.mask = TVIF_TEXT;
                    cins.item.pszText = const_cast<wchar_t*>(cLabel.c_str());
                    HTREEITEM ch = (HTREEITEM)SendMessageW(m_tree, TVM_INSERTITEMW, 0, (LPARAM)&cins);
                    for (size_t ei : items) insertEntryNode(ch, ei);
                };

                insCat(L"Dialogue Voice Clips", dialogueFlc);
                insCat(L"Other FLC Audio", otherFlc);
                continue;
            }

            for (const auto& cat : catOrder) {
                auto it = txtCategories.find(cat);
                if (it == txtCategories.end() || it->second.empty()) continue;

                SortBsaCategoryEntries(a, cat, it->second);

                std::wstring cLabel = cat + L" (" + std::to_wstring(it->second.size()) + L")";
                TVINSERTSTRUCTW cins{};
                cins.hParent = ah;
                cins.hInsertAfter = TVI_LAST;
                cins.item.mask = TVIF_TEXT;
                cins.item.pszText = const_cast<wchar_t*>(cLabel.c_str());
                HTREEITEM ch = (HTREEITEM)SendMessageW(m_tree, TVM_INSERTITEMW, 0, (LPARAM)&cins);

                for (size_t ei : it->second) insertEntryNode(ch, ei);
            }

            if (!nonTxt.empty()) {
                std::stable_sort(nonTxt.begin(), nonTxt.end(), [&](size_t lhs, size_t rhs) {
                    std::wstring ln = BaseNameOnly(winutil::WidenUtf8(a.entries[lhs].name));
                    std::wstring rn = BaseNameOnly(winutil::WidenUtf8(a.entries[rhs].name));
                    return ln < rn;
                });

                std::wstring cLabel = L"Non-TXT (" + std::to_wstring(nonTxt.size()) + L")";
                TVINSERTSTRUCTW cins{};
                cins.hParent = ah;
                cins.hInsertAfter = TVI_LAST;
                cins.item.mask = TVIF_TEXT;
                cins.item.pszText = const_cast<wchar_t*>(cLabel.c_str());
                HTREEITEM ch = (HTREEITEM)SendMessageW(m_tree, TVM_INSERTITEMW, 0, (LPARAM)&cins);
                for (size_t ei : nonTxt) insertEntryNode(ch, ei);
            }
        }
    }

    struct Bucket { std::wstring label; std::vector<uint16_t> ids; };
    std::vector<Bucket> buckets;

    auto getBucket = [&](const std::wstring& label) -> Bucket& {
        for (auto& b : buckets) {
            if (_wcsicmp(b.label.c_str(), label.c_str()) == 0) return b;
        }
        buckets.push_back(Bucket{ label, {} });
        return buckets.back();
    };

    std::vector<std::pair<uint16_t, std::wstring>> bookEntries;

    for (auto& r : m_text.records) {
        std::wstring label;
        if (m_indexLoaded) {
            auto v = m_index.LabelFor(r.recordId);
            label.assign(v.begin(), v.end());
        }
        if (label.empty()) label = L"Uncategorized";

        if (LooksLikeBookRecord(r, m_text.fileBytes, label)) {
            std::wstring title = DeriveBookTitle(r, m_text.fileBytes, label);
            m_bookRecordIds.insert(r.recordId);
            m_bookRecordTitles[r.recordId] = title;
            bookEntries.push_back({ r.recordId, title });
            continue;
        }

        getBucket(label).ids.push_back(r.recordId);
    }

    if (m_indexLoaded && !m_index.labelOrder.empty()) {
        std::vector<Bucket> ordered;
        ordered.reserve(buckets.size());
        for (const auto& want : m_index.labelOrder) {
            for (auto& b : buckets) {
                if (_wcsicmp(b.label.c_str(), want.c_str()) == 0) {
                    ordered.push_back(std::move(b));
                    b.label.clear();
                    break;
                }
            }
        }
        for (auto& b : buckets) {
            if (!b.label.empty()) ordered.push_back(std::move(b));
        }
        buckets = std::move(ordered);
    }

    struct GroupNode { std::wstring label; HTREEITEM h{}; size_t count{}; };
    std::vector<GroupNode> groups;

    auto getGroup = [&](const std::wstring& glabel) -> GroupNode& {
        for (auto& g : groups) {
            if (_wcsicmp(g.label.c_str(), glabel.c_str()) == 0) return g;
        }
        groups.push_back(GroupNode{ glabel, nullptr, 0 });
        return groups.back();
    };

    for (auto& b : buckets) {
        auto glabel = TopicGroupForLabel(b.label);
        auto& g = getGroup(glabel);
        g.count += b.ids.size();
    }

    auto groupRank = [&](const std::wstring& glabel) -> int {
        if (_wcsicmp(glabel.c_str(), L"Dialogue") == 0) return 0;
        if (_wcsicmp(glabel.c_str(), L"UI Messages") == 0) return 1;
        if (_wcsicmp(glabel.c_str(), L"Data Tables") == 0) return 2;
        if (_wcsicmp(glabel.c_str(), L"Other") == 0) return 3;
        return 4;
    };

    std::sort(groups.begin(), groups.end(), [&](const GroupNode& a, const GroupNode& b) {
        int ra = groupRank(a.label), rb = groupRank(b.label);
        if (ra != rb) return ra < rb;
        return _wcsicmp(a.label.c_str(), b.label.c_str()) < 0;
    });

    if (!bookEntries.empty()) {
        std::sort(bookEntries.begin(), bookEntries.end(), [](const auto& a, const auto& b) {
            int cmp = _wcsicmp(a.second.c_str(), b.second.c_str());
            if (cmp != 0) return cmp < 0;
            return a.first < b.first;
        });

        std::wstring booksText = L"Books (" + std::to_wstring(bookEntries.size()) + L")";
        TVINSERTSTRUCTW bi{};
        bi.hParent = m_treeRootText;
        bi.hInsertAfter = TVI_LAST;
        bi.item.mask = TVIF_TEXT | TVIF_PARAM;
        bi.item.pszText = const_cast<wchar_t*>(booksText.c_str());
        bi.item.lParam = 0;
        HTREEITEM booksRoot = (HTREEITEM)SendMessageW(m_tree, TVM_INSERTITEMW, 0, (LPARAM)&bi);

        for (const auto& be : bookEntries) {
            m_pendingTreeIds.push_back(be.first);
            m_pendingTreeParents.push_back(booksRoot);
        }
    }

    for (auto& g : groups) {
        std::wstring t = g.label + L" (" + std::to_wstring(g.count) + L")";
        TVINSERTSTRUCTW gi{};
        gi.hParent = m_treeRootText;
        gi.hInsertAfter = TVI_LAST;
        gi.item.mask = TVIF_TEXT | TVIF_PARAM;
        gi.item.pszText = const_cast<wchar_t*>(t.c_str());
        gi.item.lParam = 0;
        g.h = (HTREEITEM)SendMessageW(m_tree, TVM_INSERTITEMW, 0, (LPARAM)&gi);
    }

    auto findGroupHandle = [&](const std::wstring& glabel) -> HTREEITEM {
        for (auto& g : groups) if (_wcsicmp(g.label.c_str(), glabel.c_str()) == 0) return g.h;
        return m_treeRootText;
    };

    for (auto& b : buckets) {
        auto glabel = TopicGroupForLabel(b.label);
        HTREEITEM gh = findGroupHandle(glabel);

        std::wstring t = b.label + L" (" + std::to_wstring(b.ids.size()) + L")";
        TVINSERTSTRUCTW ci{};
        ci.hParent = gh;
        ci.hInsertAfter = TVI_LAST;
        ci.item.mask = TVIF_TEXT | TVIF_PARAM;
        ci.item.pszText = const_cast<wchar_t*>(t.c_str());
        ci.item.lParam = 0;
        HTREEITEM topicH = (HTREEITEM)SendMessageW(m_tree, TVM_INSERTITEMW, 0, (LPARAM)&ci);

        std::sort(b.ids.begin(), b.ids.end());
        for (auto id : b.ids) {
            m_pendingTreeIds.push_back(id);
            m_pendingTreeParents.push_back(topicH);
        }
    }

    
    // Prepare quest insertion queue (group by guild code when possible).
    m_pendingQuestIdx.clear();
    m_pendingQuestParents.clear();
    m_questInsertPos = 0;

    if (m_questsLoaded && m_treeRootQuests) {
        struct QBucket { std::wstring label; std::vector<size_t> idx; };
        std::vector<QBucket> qb;

        auto getQ = [&](const std::wstring& label) -> QBucket& {
            for (auto& b : qb) if (_wcsicmp(b.label.c_str(), label.c_str()) == 0) return b;
            qb.push_back(QBucket{ label, {} });
            return qb.back();
        };

        for (size_t i = 0; i < m_quests.quests.size(); ++i) {
            const auto& q = m_quests.quests[i];
            std::wstring group = L"(Other)";
            if (q.guildCode) {
                std::wstring c(1, (wchar_t)q.guildCode);
                group = c + L" - " + winutil::WidenUtf8(q.guildName);
            }
            getQ(group).idx.push_back(i);
        }

        std::sort(qb.begin(), qb.end(), [](const QBucket& a, const QBucket& b) {
            return _wcsicmp(a.label.c_str(), b.label.c_str()) < 0;
        });

        for (auto& b : qb) {
            TVINSERTSTRUCTW g{};
            g.hParent = m_treeRootQuests;
            g.hInsertAfter = TVI_LAST;
            g.item.mask = TVIF_TEXT;
            g.item.pszText = const_cast<wchar_t*>(b.label.c_str());
            HTREEITEM hGroup = (HTREEITEM)SendMessageW(m_tree, TVM_INSERTITEMW, 0, (LPARAM)&g);

            std::sort(b.idx.begin(), b.idx.end());
            for (auto qi : b.idx) {
                m_pendingQuestIdx.push_back(qi);
                m_pendingQuestParents.push_back(hGroup);
            }
        }
    }
wchar_t sb[256]{};
    swprintf_s(sb, L"Index loaded. Building Object Window... 0 / %zu (Quests: %zu)", m_pendingTreeIds.size(), m_pendingQuestIdx.size());
    SetStatus(sb);

    SetTimer(m_hwnd, TIMER_POP_TREE, 1, nullptr);
}

void MainWindow::TreeBuildTick() {
    const size_t totalText = m_pendingTreeIds.size();
    const size_t totalQuest = m_pendingQuestIdx.size();
    const size_t batch = 400;

    // Phase 1: TEXT.RSC records
    if (m_treeInsertPos < totalText) {
        size_t end = std::min(totalText, m_treeInsertPos + batch);
        for (size_t i = m_treeInsertPos; i < end; ++i) {
            uint16_t id = m_pendingTreeIds[i];
            HTREEITEM parent = (i < m_pendingTreeParents.size()) ? m_pendingTreeParents[i] : m_treeRootText;

            std::wstring label = FormatRecLabel(id);
            auto bt = m_bookRecordTitles.find(id);
            if (bt != m_bookRecordTitles.end()) {
                label = FormatBookRecLabel(id, bt->second);
            }

            TVINSERTSTRUCTW recIns{};
            recIns.hParent = parent;
            recIns.hInsertAfter = TVI_LAST;
            recIns.item.mask = TVIF_TEXT | TVIF_PARAM;
            recIns.item.pszText = const_cast<wchar_t*>(label.c_str());
            recIns.item.lParam = (LPARAM)AddPayload(TreePayload::Kind::TextRecord, id);
            SendMessageW(m_tree, TVM_INSERTITEMW, 0, (LPARAM)&recIns);
        }

        m_treeInsertPos = end;

        wchar_t sb[256]{};
        swprintf_s(sb, L"Building Object Window... %zu / %zu", m_treeInsertPos, totalText);
        SetStatus(sb);

        if (m_treeInsertPos >= totalText) {
            // fallthrough to phase 2
        } else {
            return;
        }
    }

    // Phase 2: Quests
    if (m_questInsertPos < totalQuest) {
        size_t endQ = std::min(totalQuest, m_questInsertPos + batch);
        for (size_t i = m_questInsertPos; i < endQ; ++i) {
            size_t qidx = m_pendingQuestIdx[i];
            HTREEITEM parent = (i < m_pendingQuestParents.size()) ? m_pendingQuestParents[i] : m_treeRootQuests;
            const auto& q = m_quests.quests[qidx];
            std::wstring label = winutil::WidenUtf8(q.baseName) + L" - " + winutil::WidenUtf8(!q.displayName.empty() ? q.displayName : q.guildName);

            TVINSERTSTRUCTW ins{};
            ins.hParent = parent;
            ins.hInsertAfter = TVI_LAST;
            ins.item.mask = TVIF_TEXT | TVIF_PARAM;
            ins.item.pszText = const_cast<wchar_t*>(label.c_str());
            ins.item.lParam = (LPARAM)AddPayload(TreePayload::Kind::Quest, 0, qidx);
            (void)SendMessageW(m_tree, TVM_INSERTITEMW, 0, (LPARAM)&ins);
        }

        m_questInsertPos = endQ;

        wchar_t sb[256]{};
        swprintf_s(sb, L"Building Quests... %zu / %zu", m_questInsertPos, totalQuest);
        SetStatus(sb);
        return;
    }

    KillTimer(m_hwnd, TIMER_POP_TREE);
    TreeView_Expand(m_tree, m_treeRootText, TVE_EXPAND);
    TreeView_Expand(m_tree, m_treeRootQuests, TVE_EXPAND);
    TreeView_Expand(m_tree, m_treeRootBsa, TVE_EXPAND);

    SetStatus(L"Ready.");
}

void MainWindow::PopulateSubrecordList(arena2::TextRecord& rec) {
    rec.EnsureParsed(m_text.fileBytes);

    ListView_DeleteAllItems(m_list);

    for (int i = 0; i < (int)rec.subrecords.size(); ++i) {
        const auto& sr = rec.subrecords[i];

        std::wstring idx = std::to_wstring(i);
        std::string prev;
        if (sr.hasUserOverride) prev = sr.userOverride;
        else prev = CheapPreview(sr.raw);
        if (prev.size() > 220) prev.resize(220);
        for (auto& ch : prev) if (ch == '\r' || ch == '\n' || ch == '\t') ch = ' ';
        std::wstring prevW = winutil::WidenUtf8(prev);

        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = i;
        item.pszText = const_cast<wchar_t*>(idx.c_str());
        item.lParam = (LPARAM)i;
        ListView_InsertItem(m_list, &item);

        ListView_SetItemText(m_list, i, 1, const_cast<wchar_t*>(prevW.c_str()));
    }

    if (!rec.subrecords.empty()) {
        ListView_SetItemState(m_list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ShowSubrecordPreview(rec, 0);
    } else {
        SetWindowTextW(m_preview, L"");
    }
}

void MainWindow::ShowSubrecordPreview(arena2::TextRecord& rec, int index) {
    rec.EnsureParsed(m_text.fileBytes);
    if (index < 0 || index >= (int)rec.subrecords.size()) return;

    auto& sr = rec.subrecords[index];
    auto& tok = sr.EnsureTokens();

    NoteDiscoveredVars(tok);

    const std::string& base = sr.hasUserOverride ? sr.userOverride : tok.rich;
    std::string rich = ApplyOverrides(base);
    std::wstring text = winutil::WidenUtf8(rich);
    SetWindowTextW(m_preview, text.c_str());

    wchar_t buf[256]{};
    swprintf_s(buf, L"Record 0x%04X / Subrecord %d | Tokens: %u | Vars: %u", (unsigned)rec.recordId, index, (unsigned)tok.tokens.size(), (unsigned)tok.vars.size());
    SetStatus(buf);
}


static std::wstring TrimOneLineW(std::wstring s, size_t maxChars) {
    for (auto& ch : s) {
        if (ch == L'\r' || ch == L'\n' || ch == L'\t') ch = L' ';
    }
    // trim
    while (!s.empty() && s.front() == L' ') s.erase(s.begin());
    while (!s.empty() && s.back() == L' ') s.pop_back();
    if (s.size() > maxChars) {
        s.resize(maxChars);
        if (!s.empty()) s.back() = (wchar_t)0x2026; // ellipsis
    }
    return s;
}

void MainWindow::BeginListPreviewEdit(int item, int subItem) {
    if (!m_list) return;

    // Only enable on Preview column, depending on current view mode.
    int wantSub = -1;
    if (m_viewMode == ViewMode::TextSubrecords) wantSub = 1;
    else if (m_viewMode == ViewMode::QuestText) wantSub = 2;
    else return;

    if (item < 0 || subItem != wantSub) return;

    // If an editor is already active, commit it first.
    EndListPreviewEdit(true);

    RECT rc{};
    if (!ListView_GetSubItemRect(m_list, item, subItem, LVIR_LABEL, &rc)) return;
    if (rc.right <= rc.left || rc.bottom <= rc.top) return;

    rc.left += 1;
    rc.top += 1;
    rc.right -= 1;
    rc.bottom -= 1;

    wchar_t buf[4096]{};
    ListView_GetItemText(m_list, item, subItem, buf, 4096);
    std::wstring initial = buf;

    m_inplaceItem = item;
    m_inplaceSubItem = subItem;

    m_inplaceEdit = CreateWindowExW(
        0, L"EDIT", initial.c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
        m_list, nullptr, GetModuleHandleW(nullptr), nullptr
    );
    if (!m_inplaceEdit) {
        m_inplaceItem = -1;
        m_inplaceSubItem = -1;
        return;
    }

    HFONT hFont = (HFONT)SendMessageW(m_list, WM_GETFONT, 0, 0);
    if (hFont) SendMessageW(m_inplaceEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    SetWindowLongPtrW(m_inplaceEdit, GWLP_USERDATA, (LONG_PTR)this);
    m_inplaceEditOldProc = (WNDPROC)SetWindowLongPtrW(m_inplaceEdit, GWLP_WNDPROC, (LONG_PTR)&MainWindow::InplaceEditProc);

    SendMessageW(m_inplaceEdit, EM_SETSEL, 0, -1);
    SetFocus(m_inplaceEdit);
}

void MainWindow::EndListPreviewEdit(bool commit) {
    if (!m_inplaceEdit) return;
    if (m_inplaceEnding) return;

    m_inplaceEnding = true;

    std::wstring newText;
    if (commit) {
        int len = GetWindowTextLengthW(m_inplaceEdit);
        if (len < 0) len = 0;
        std::wstring tmp;
        tmp.resize((size_t)len + 1);
        if (len > 0) GetWindowTextW(m_inplaceEdit, tmp.data(), len + 1);
        tmp.resize(wcslen(tmp.c_str()));
        newText = std::move(tmp);
    }

    HWND hEdit = m_inplaceEdit;
    m_inplaceEdit = nullptr;

    if (IsWindow(hEdit)) {
        // Restore proc to avoid re-entrancy weirdness during destroy.
        if (m_inplaceEditOldProc) SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)m_inplaceEditOldProc);
        DestroyWindow(hEdit);
    }

    int item = m_inplaceItem;
    int sub = m_inplaceSubItem;

    m_inplaceItem = -1;
    m_inplaceSubItem = -1;
    m_inplaceEditOldProc = nullptr;

    m_inplaceEnding = false;

    if (commit) CommitListPreviewEdit(item, sub, newText);
}

void MainWindow::CommitListPreviewEdit(int item, int subItem, const std::wstring& newText) {
    (void)subItem;
    std::wstring cleaned = TrimOneLineW(newText, 4096);

    // Apply to model depending on view mode.
    if (m_viewMode == ViewMode::TextSubrecords) {
        auto* pld = GetSelectedPayload();
        if (!pld || pld->kind != TreePayload::Kind::TextRecord) return;
        auto* rec = m_text.FindMutable(pld->textRecordId);
        if (!rec) return;
        rec->EnsureParsed(m_text.fileBytes);

        const int idx = item;
        if (idx < 0 || idx >= (int)rec->subrecords.size()) return;

        rec->subrecords[idx].SetOverride(winutil::NarrowUtf8(cleaned));

        // Update list cell
        std::wstring cell = TrimOneLineW(cleaned, 220);
        ListView_SetItemText(m_list, idx, 1, const_cast<LPWSTR>(cell.c_str()));

        ShowSubrecordPreview(*rec, idx);
        return;
    }

    if (m_viewMode == ViewMode::QuestText) {
        if (!m_questsLoaded || m_activeQuest == (size_t)-1 || m_activeQuest >= m_quests.quests.size()) return;

        auto& q = m_quests.quests[m_activeQuest];
        if (!q.qrcLoaded) {
            std::wstring err;
            if (!m_quests.EnsureQrcLoaded(m_activeQuest, &err)) return;
        }

        const int row = item;
        if (row < 0 || row >= (int)q.qrc.records.size()) return;

        auto& rec = q.qrc.records[row];
        rec.EnsureParsed(q.qrc.fileBytes);

        if (rec.subrecords.empty()) {
            arena2::TextSubrecord sr{};
            sr.SetOverride(winutil::NarrowUtf8(cleaned));
            rec.subrecords.push_back(std::move(sr));
        } else {
            rec.subrecords[0].SetOverride(winutil::NarrowUtf8(cleaned));
        }

        std::wstring cell = TrimOneLineW(cleaned, 96);
        ListView_SetItemText(m_list, row, 2, const_cast<LPWSTR>(cell.c_str()));

        ShowQuestTextPreview(m_activeQuest, row);
        return;
    }
}

LRESULT CALLBACK MainWindow::InplaceEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = (MainWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    WNDPROC oldp = self ? self->m_inplaceEditOldProc : nullptr;

    switch (msg) {
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            if (self) self->EndListPreviewEdit(true);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            if (self) self->EndListPreviewEdit(false);
            return 0;
        }
        break;
    case WM_KILLFOCUS:
        if (self) self->EndListPreviewEdit(true);
        break;
    default:
        break;
    }

    return oldp ? CallWindowProcW(oldp, hwnd, msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
}



void MainWindow::OnTreeSelChanged() {
    auto* p = GetSelectedPayload();
    if (!p) return;

    m_bsaDialogueKind = BsaDialogueKind::None;
    m_bsaDialogueRows.clear();
    m_bsaDialogueHeaders.clear();
    m_bsaDialogueTitle.clear();

    if (p->kind == TreePayload::Kind::TextRecord) {
        ShowQuestView(false);
        SetupListColumns_TextSubrecords();

        auto* rec = m_text.FindMutable(p->textRecordId);
        if (!rec) return;
        PopulateSubrecordList(*rec);

        if (!rec->subrecords.empty()) ShowSubrecordPreview(*rec, 0);
        return;
    }

    if (p->kind == TreePayload::Kind::Quest) {
        ActivateQuest(p->questIndex);
        return;
    }

    if (p->kind == TreePayload::Kind::BsaEntry) {
        SetupListColumns_TextSubrecords();
        ListView_DeleteAllItems(m_list);

        if (p->bsaArchiveIndex >= m_bsaArchives.size()) return;
        const auto& ar = m_bsaArchives[p->bsaArchiveIndex];
        if (p->bsaEntryIndex >= ar.entries.size()) return;
        const auto& e = ar.entries[p->bsaEntryIndex];

        std::vector<uint8_t> bytes;
        std::wstring err;
        if (!ar.ReadEntryData(e, bytes, &err)) {
            SetWindowTextW(m_preview, (L"Failed to decode BSA entry: " + err).c_str());
            return;
        }

        std::wstring preview;
        bool renderedTable = false;
        if (IsLikelyUtf8Printable(bytes)) {
            std::string text(bytes.begin(), bytes.end());
            if (text.size() > 131072) text.resize(131072);
            preview = winutil::WidenUtf8(text);

            std::wstring nameLow = ToLowerWs(winutil::WidenUtf8(e.name));
            bool tableLikeExt = EndsWithWs(nameLow, L".xls") || EndsWithWs(nameLow, L".txt") || EndsWithWs(nameLow, L".csv");
            if (tableLikeExt && preview.find(L'	') != std::wstring::npos) {
                auto rows = ParseTabLines(preview);
                if (rows.size() >= 2) {
                    size_t maxCols = 0;
                    for (const auto& r : rows) maxCols = std::max(maxCols, r.size());
                    if (maxCols >= 3) {
                        size_t headerRow = 0;
                        size_t bestScore = 0;
                        bool dialogueScript = false;
                        bool dialogueRouting = false;

                        for (size_t ri = 0; ri < rows.size(); ++ri) {
                            size_t nonEmpty = CountNonEmptyCells(rows[ri]);
                            size_t scriptHits = 0;
                            size_t routingHits = 0;
                            for (const auto& c : rows[ri]) {
                                if (IsDialogueScriptHeaderCell(c)) scriptHits++;
                                if (IsRoutingHeaderCell(c)) routingHits++;
                            }

                            size_t score = nonEmpty + scriptHits * 100 + routingHits * 70;
                            if (score > bestScore) { bestScore = score; headerRow = ri; }

                            if (scriptHits >= 2) dialogueScript = true;
                            if (routingHits >= 1 || (routingHits == 0 && nonEmpty >= 2)) {
                                for (const auto& c : rows[ri]) {
                                    if (IsRoutingKeywordCell(c)) {
                                        dialogueRouting = true;
                                        break;
                                    }
                                }
                            }
                        }

                        std::vector<std::wstring> headers;
                        headers.reserve(maxCols);
                        for (size_t ci = 0; ci < maxCols; ++ci) {
                            std::wstring hn = (ci < rows[headerRow].size() && !rows[headerRow][ci].empty()) ? rows[headerRow][ci] : (L"Col" + std::to_wstring(ci + 1));
                            headers.push_back(std::move(hn));
                        }

                        if (dialogueScript) {
                            SetupListColumns_BsaDialogueScript();
                            m_bsaDialogueKind = BsaDialogueKind::Script;
                        } else if (dialogueRouting) {
                            SetupListColumns_BsaDialogueRouting();
                            m_bsaDialogueKind = BsaDialogueKind::Routing;
                        } else {
                            SetupListColumns_BsaTable(headers);
                            m_bsaDialogueKind = BsaDialogueKind::Table;
                        }

                        ListView_DeleteAllItems(m_list);
                        m_bsaDialogueHeaders = headers;
                        m_bsaDialogueRows.clear();
                        m_bsaDialogueTitle = winutil::WidenUtf8(e.name);

                        int outRow = 0;
                        for (size_t ri = headerRow + 1; ri < rows.size(); ++ri) {
                            bool any = CountNonEmptyCells(rows[ri]) > 0;
                            if (!any) continue;

                            m_bsaDialogueRows.push_back(rows[ri]);
                            const size_t srcIdx = m_bsaDialogueRows.size() - 1;

                            LVITEMW it{};
                            it.mask = LVIF_TEXT | LVIF_PARAM;
                            it.iItem = outRow;
                            it.lParam = (LPARAM)srcIdx;

                            std::wstring first = (rows[ri].empty() ? L"" : rows[ri][0]);
                            std::wstring second = (rows[ri].size() > 1 ? rows[ri][1] : L"");
                            std::wstring third = (rows[ri].size() > 2 ? rows[ri][2] : L"");

                            if (m_bsaDialogueKind == BsaDialogueKind::Script) {
                                it.pszText = const_cast<wchar_t*>(first.c_str());
                                ListView_InsertItem(m_list, &it);
                                ListView_SetItemText(m_list, outRow, 1, const_cast<wchar_t*>(second.c_str()));
                                ListView_SetItemText(m_list, outRow, 2, const_cast<wchar_t*>(third.c_str()));
                            } else if (m_bsaDialogueKind == BsaDialogueKind::Routing) {
                                it.pszText = const_cast<wchar_t*>(first.c_str());
                                ListView_InsertItem(m_list, &it);
                                ListView_SetItemText(m_list, outRow, 1, const_cast<wchar_t*>(second.c_str()));
                                std::wstring notes = (rows[ri].size() > 3 ? rows[ri][3] : L"");
                                if (notes.empty()) notes = third;
                                ListView_SetItemText(m_list, outRow, 2, const_cast<wchar_t*>(notes.c_str()));
                            } else {
                                it.pszText = const_cast<wchar_t*>(first.c_str());
                                ListView_InsertItem(m_list, &it);
                                for (size_t ci = 1; ci < maxCols; ++ci) {
                                    std::wstring cell = (ci < rows[ri].size()) ? rows[ri][ci] : L"";
                                    ListView_SetItemText(m_list, outRow, (int)ci, const_cast<wchar_t*>(cell.c_str()));
                                }
                            }

                            outRow++;
                            if (outRow > 4000) break;
                        }

                        renderedTable = (outRow > 0);
                    }
                }
            }
        } else {
            preview = HexDumpPreview(bytes);
        }

        if (!renderedTable) {
            SetupListColumns_TextSubrecords();
            ListView_DeleteAllItems(m_list);
            auto addRow = [&](int idx, const std::wstring& text) {
                LVITEMW it{};
                it.mask = LVIF_TEXT;
                it.iItem = idx;
                std::wstring i = std::to_wstring(idx);
                it.pszText = const_cast<wchar_t*>(i.c_str());
                ListView_InsertItem(m_list, &it);
                ListView_SetItemText(m_list, idx, 1, const_cast<wchar_t*>(text.c_str()));
            };
            addRow(0, L"Name: " + winutil::WidenUtf8(e.name));
            addRow(1, L"Offset: 0x" + HexU32(e.offset));
            addRow(2, L"Packed Size: " + std::to_wstring(e.packedSize));
            addRow(3, L"Compression Flag: " + std::to_wstring(e.compressionFlag));
            addRow(4, L"Decoded Size: " + std::to_wstring(bytes.size()));
        }

        if (renderedTable) {
            ShowBsaDialogueRowPreview(0);
        } else {
            SetWindowTextW(m_preview, preview.c_str());
        }
        m_viewMode = ViewMode::BsaEntry;
        return;
    }

    // Root/group nodes
    ShowQuestView(false);
    ListView_DeleteAllItems(m_list);
    SetWindowTextW(m_preview, L"");
}



void MainWindow::CmdExtractBsa() {
    auto picked = winutil::PickFile(m_hwnd,
        L"Select Battlespire BSA to extract",
        L"Battlespire Archives (*.bsa)|*.bsa|All Files (*.*)|*.*");
    if (!picked) return;

    std::filesystem::path bsaPath = *picked;
    battlespire::BsaArchive archive;
    std::wstring err;
    if (!battlespire::BsaArchive::LoadFromFile(bsaPath, archive, &err)) {
        MessageBoxW(m_hwnd, (L"Failed to open BSA: " + err).c_str(), L"Extract BSA", MB_OK | MB_ICONERROR);
        return;
    }

    std::filesystem::path outRoot = winutil::GetExeDirectory();
    std::filesystem::path outDir = outRoot / (bsaPath.stem().wstring() + L"_extracted");

    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    if (ec) {
        MessageBoxW(m_hwnd, (L"Failed to create output folder: " + outDir.wstring()).c_str(), L"Extract BSA", MB_OK | MB_ICONERROR);
        return;
    }

    size_t okCount = 0;
    size_t failCount = 0;
    for (size_t i = 0; i < archive.entries.size(); ++i) {
        const auto& e = archive.entries[i];

        std::vector<uint8_t> bytes;
        std::wstring derr;
        if (!archive.ReadEntryData(e, bytes, &derr)) {
            failCount++;
            continue;
        }

        std::wstring name = winutil::WidenUtf8(e.name);
        if (name.empty()) name = L"entry_" + std::to_wstring(i);
        for (auto& c : name) {
            if (c == L'/' || c == L'\\' || c == L':' || c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|') c = L'_';
        }

        std::filesystem::path outPath = outDir / name;
        std::ofstream f(outPath, std::ios::binary);
        if (!f) { failCount++; continue; }
        if (!bytes.empty()) f.write(reinterpret_cast<const char*>(bytes.data()), (std::streamsize)bytes.size());
        if (!f.good()) { failCount++; continue; }
        okCount++;
    }

    std::wstring msg = L"Extracted " + std::to_wstring(okCount) + L" entries to:\n" + outDir.wstring();
    if (failCount) msg += L"\n\nFailed entries: " + std::to_wstring(failCount);
    MessageBoxW(m_hwnd, msg.c_str(), L"Extract BSA", MB_OK | (failCount ? MB_ICONWARNING : MB_ICONINFORMATION));
    SetStatus(L"BSA extraction complete: " + std::to_wstring(okCount) + L" files");
}

void MainWindow::CmdOpenSpire() {
    if (m_loading.load()) return;

    auto folder = winutil::PickFolder(m_hwnd, L"Select Battlespire folder (or game root containing GameData)");
    if (!folder) return;

    m_loading.store(true);
    SetStatus(L"Loading Battlespire TEXT.RSC...");
    SetCursor(LoadCursorW(nullptr, IDC_WAIT));

    HMENU hMenu = GetMenu(m_hwnd);
    EnableMenuItem(hMenu, IDM_FILE_OPEN_SPIRE, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hMenu, IDM_FILE_OPEN_ARENA2, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hMenu, IDM_EXPORT_SUBRECORDS, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hMenu, IDM_EXPORT_TOKENS, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hMenu, IDM_EXPORT_VARIABLES, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hMenu, IDM_EXPORT_QUESTS, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hMenu, IDM_EXPORT_QUEST_STAGES, MF_BYCOMMAND | MF_GRAYED);
    DrawMenuBar(m_hwnd);

    auto spirePath = *folder;
    std::thread([hwnd = m_hwnd, spirePath]() {
        auto* r = new LoadResult();
        r->questsOk = false;

        // Load hash catalog for resolving quest state variable names.
        arena2::VarHashCatalog qhash;
        {
            std::filesystem::path exeDir = winutil::GetExeDirectory();
            std::filesystem::path p1 = exeDir / L"TEXT_VARIABLE_HASHES.txt";
            std::filesystem::path p2 = exeDir / L"data" / L"TEXT_VARIABLE_HASHES.txt";
            std::wstring herr;
            if (std::filesystem::exists(p1)) qhash.LoadFromFile(p1, &herr);
            else if (std::filesystem::exists(p2)) qhash.LoadFromFile(p2, &herr);
        }

        std::wstring err;
        arena2::TextRsc loaded;
        if (arena2::TextRsc::LoadFromBattlespireRoot(spirePath, loaded, &err)) {
            r->ok = true;
            r->text = std::move(loaded);

            std::wstring qerr;
            if (r->quests.LoadFromBattlespireRoot(spirePath, &qhash, &qerr)) {
                r->questsOk = true;
            } else {
                r->questsOk = false;
            }

            // Auto-discover and load Battlespire BSA archives for browsing.
            std::vector<std::filesystem::path> roots = { spirePath, spirePath / L"GameData" };
            std::unordered_set<std::wstring> seen;
            for (const auto& root : roots) {
                if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) continue;
                for (auto& it : std::filesystem::directory_iterator(root)) {
                    if (!it.is_regular_file()) continue;
                    auto ext = it.path().extension().wstring();
                    for (auto& c : ext) c = (wchar_t)towupper(c);
                    if (ext != L".BSA") continue;
                    auto canon = it.path().wstring();
                    if (!seen.insert(canon).second) continue;

                    battlespire::BsaArchive arc;
                    std::wstring berr;
                    if (battlespire::BsaArchive::LoadFromFile(it.path(), arc, &berr)) {
                        r->bsaArchives.push_back(std::move(arc));
                    }
                }
            }
            r->bsaOk = !r->bsaArchives.empty();
        }
        else {
            r->ok = false;
            r->err = err;
        }

        PostMessageW(hwnd, WM_APP_LOAD_DONE, (WPARAM)r, 0);
    }).detach();
}

void MainWindow::CmdOpenArena2() {
    if (m_loading.load()) return;

    auto folder = winutil::PickFolder(m_hwnd, L"Select Daggerfall ARENA2 folder (or game root containing ARENA2)");
    if (!folder) return;

    m_loading.store(true);
    SetStatus(L"Loading TEXT.RSC...");
    SetCursor(LoadCursorW(nullptr, IDC_WAIT));

    HMENU hMenu = GetMenu(m_hwnd);
    EnableMenuItem(hMenu, IDM_FILE_OPEN_ARENA2, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hMenu, IDM_EXPORT_SUBRECORDS, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hMenu, IDM_EXPORT_TOKENS, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hMenu, IDM_EXPORT_VARIABLES, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hMenu, IDM_EXPORT_QUESTS, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hMenu, IDM_EXPORT_QUEST_STAGES, MF_BYCOMMAND | MF_GRAYED);
    DrawMenuBar(m_hwnd);

    auto arenaPath = *folder;

    std::thread([hwnd = m_hwnd, arenaPath]() {
        auto* r = new LoadResult();

        // Load hash catalog for resolving quest state variable names.
        arena2::VarHashCatalog qhash;
        {
            std::filesystem::path exeDir = winutil::GetExeDirectory();
            std::filesystem::path p1 = exeDir / L"TEXT_VARIABLE_HASHES.txt";
            std::filesystem::path p2 = exeDir / L"data" / L"TEXT_VARIABLE_HASHES.txt";
            std::wstring herr;
            if (std::filesystem::exists(p1)) qhash.LoadFromFile(p1, &herr);
            else if (std::filesystem::exists(p2)) qhash.LoadFromFile(p2, &herr);
        }

        std::wstring err;
        arena2::TextRsc loaded;
        if (arena2::TextRsc::LoadFromArena2Root(arenaPath, loaded, &err)) {
            r->ok = true;
            r->text = std::move(loaded);

            std::wstring qerr;
            if (r->quests.LoadFromArena2Root(arenaPath, &qhash, &qerr)) {
                r->questsOk = true;
            } else {
                r->questsOk = false;
            }
        } else {
            r->ok = false;
            r->err = err;
        }

        PostMessageW(hwnd, WM_APP_LOAD_DONE, (WPARAM)r, 0);
    }).detach();
}

static std::string U16Hex(uint16_t v) {
    char buf[16]{};
    snprintf(buf, sizeof(buf), "0x%04X", (unsigned)v);
    return std::string(buf);
}

static std::string TokenTypeToString(arena2::TokenType t) {
    switch (t) {
    case arena2::TokenType::Text: return "Text";
    case arena2::TokenType::NewLine: return "NewLine";
    case arena2::TokenType::Position: return "Position";
    case arena2::TokenType::EndOfPage: return "EndOfPage";
    case arena2::TokenType::EndOfLineLeft: return "EndOfLineLeft";
    case arena2::TokenType::EndOfLineCenter: return "EndOfLineCenter";
    case arena2::TokenType::Font: return "Font";
    case arena2::TokenType::Color: return "Color";
    case arena2::TokenType::BookImage: return "BookImage";
    default: return "Unknown";
    }
}

void MainWindow::CmdExportSubrecords() {
    if (m_text.records.empty()) {
        MessageBoxW(m_hwnd, L"No data loaded.", L"Export", MB_OK | MB_ICONWARNING);
        return;
    }

    auto folder = winutil::PickFolder(m_hwnd, L"Select export folder");
    if (!folder) return;

    std::string out;
    out.reserve(1024 * 1024);
    csv::AppendRow(out, { "RecordId", "Group", "Category", "SubrecordIndex", "SubrecordCount", "PlainText", "RichText", "TokenCount", "HasEndOfPage", "HasFontScript" });

    for (auto& r : m_text.records) {
        r.EnsureParsed(m_text.fileBytes);

        std::wstring wcat;
        if (m_indexLoaded) {
            auto v = m_index.LabelFor(r.recordId);
            wcat.assign(v.begin(), v.end());
        }
        if (wcat.empty()) wcat = L"Uncategorized";

        std::string cat = winutil::NarrowUtf8(wcat);
        std::string grp = winutil::NarrowUtf8(TopicGroupForLabel(wcat));

        for (size_t i = 0; i < r.subrecords.size(); ++i) {
            auto& sr = r.subrecords[i];
            auto& tok = sr.EnsureTokens();
            const std::string& basePlain = sr.hasUserOverride ? sr.userOverride : tok.plain;
            const std::string& baseRich  = sr.hasUserOverride ? sr.userOverride : tok.rich;

            csv::AppendRow(out, {
                U16Hex(r.recordId),
                grp,
                cat,
                std::to_string(i),
                std::to_string(r.subrecords.size()),
                ApplyOverrides(basePlain),
                ApplyOverrides(baseRich),
                std::to_string(tok.tokens.size()),
                tok.hasEndOfPage ? "1" : "0",
                tok.hasFontScript ? "1" : "0"
            });
        }

        if (r.subrecords.empty()) {
            csv::AppendRow(out, { U16Hex(r.recordId), grp, cat, "0", "0", "", "", "0", "0", "0" });
        }
    }

    std::wstring err;
    auto path = *folder / "TEXT_RSC_Subrecords.csv";
    if (!csv::WriteUtf8File(path, out, &err)) {
        MessageBoxW(m_hwnd, err.c_str(), L"Export failed", MB_OK | MB_ICONERROR);
        return;
    }
    SetStatus(L"Exported TEXT_RSC_Subrecords.csv");
}

void MainWindow::CmdExportTokens() {
    if (m_text.records.empty()) {
        MessageBoxW(m_hwnd, L"No data loaded.", L"Export", MB_OK | MB_ICONWARNING);
        return;
    }

    auto folder = winutil::PickFolder(m_hwnd, L"Select export folder");
    if (!folder) return;

    std::string out;
    out.reserve(1024 * 1024);
    csv::AppendRow(out, { "RecordId", "Group", "Category", "SubrecordIndex", "TokenIndex", "TokenType", "Arg0", "Arg1", "Text", "ByteOffset" });

    for (auto& r : m_text.records) {
        r.EnsureParsed(m_text.fileBytes);

        std::wstring wcat;
        if (m_indexLoaded) {
            auto v = m_index.LabelFor(r.recordId);
            wcat.assign(v.begin(), v.end());
        }
        if (wcat.empty()) wcat = L"Uncategorized";

        std::string cat = winutil::NarrowUtf8(wcat);
        std::string grp = winutil::NarrowUtf8(TopicGroupForLabel(wcat));

        for (size_t si = 0; si < r.subrecords.size(); ++si) {
            auto& sr = r.subrecords[si];
            auto& tt = sr.EnsureTokens();

            for (size_t ti = 0; ti < tt.tokens.size(); ++ti) {
                const auto& tok = tt.tokens[ti];

                csv::AppendRow(out, {
                    U16Hex(r.recordId),
                    grp,
                    cat,
                    std::to_string(si),
                    std::to_string(ti),
                    TokenTypeToString(tok.type),
                    std::to_string(tok.arg0),
                    std::to_string(tok.arg1),
                    tok.text,
                    std::to_string(tok.byteOffset)
                });
            }
        }
    }

    std::wstring err;
    auto path = *folder / "TEXT_RSC_Tokens.csv";
    if (!csv::WriteUtf8File(path, out, &err)) {
        MessageBoxW(m_hwnd, err.c_str(), L"Export failed", MB_OK | MB_ICONERROR);
        return;
    }
    SetStatus(L"Exported TEXT_RSC_Tokens.csv");
}

void MainWindow::CmdExportVariables() {
    if (m_text.records.empty()) {
        MessageBoxW(m_hwnd, L"No data loaded.", L"Export", MB_OK | MB_ICONWARNING);
        return;
    }

    // Ensure hashes loaded (optional, but enriches output)
    if (!m_varHashLoaded) {
        std::filesystem::path exeDir = winutil::GetExeDirectory();
        std::filesystem::path p1 = exeDir / L"TEXT_VARIABLE_HASHES.txt";
        std::filesystem::path p2 = exeDir / L"data" / L"TEXT_VARIABLE_HASHES.txt";
        std::wstring err;
        if (std::filesystem::exists(p1)) m_varHashLoaded = m_varHash.LoadFromFile(p1, &err);
        else if (std::filesystem::exists(p2)) m_varHashLoaded = m_varHash.LoadFromFile(p2, &err);
        else m_varHashLoaded = false;
    }

    auto folder = winutil::PickFolder(m_hwnd, L"Select export folder");
    if (!folder) return;

    std::string out;
    out.reserve(1024 * 1024);
    csv::AppendRow(out, { "RecordId", "Group", "Category", "SubrecordIndex", "VarStyle", "Token", "Name", "Hash", "Candidates", "PlainOffset", "ByteOffset" });

    auto hex32 = [](uint32_t v) {
        char b[16]{};
        snprintf(b, sizeof(b), "0x%08X", (unsigned)v);
        return std::string(b);
    };

    for (auto& r : m_text.records) {
        r.EnsureParsed(m_text.fileBytes);

        std::wstring wcat;
        if (m_indexLoaded) {
            auto v = m_index.LabelFor(r.recordId);
            wcat.assign(v.begin(), v.end());
        }
        if (wcat.empty()) wcat = L"Uncategorized";

        std::string cat = winutil::NarrowUtf8(wcat);
        std::string grp = winutil::NarrowUtf8(TopicGroupForLabel(wcat));

        for (size_t si = 0; si < r.subrecords.size(); ++si) {
            auto& sr = r.subrecords[si];
            auto& tt = sr.EnsureTokens();

            for (const auto& vr : tt.vars) {
                std::string style = (vr.style == arena2::VarStyle::Percent) ? "Percent" : "Underscore";

                std::string cand;
                if (m_varHashLoaded) {
                    if (auto* names = m_varHash.NamesFor(vr.hash)) {
                        for (size_t i = 0; i < names->size(); ++i) {
                            if (i) cand.push_back('|');
                            cand.append((*names)[i]);
                        }
                    }
                }

                csv::AppendRow(out, {
                    U16Hex(r.recordId),
                    grp,
                    cat,
                    std::to_string(si),
                    style,
                    vr.token,
                    vr.name,
                    hex32(vr.hash),
                    cand,
                    std::to_string(vr.plainOffset),
                    std::to_string(vr.byteOffset)
                });
            }
        }
    }

    std::wstring err;
    auto path = *folder / "TEXT_RSC_Variables.csv";
    if (!csv::WriteUtf8File(path, out, &err)) {
        MessageBoxW(m_hwnd, err.c_str(), L"Export failed", MB_OK | MB_ICONERROR);
        return;
    }
    SetStatus(L"Exported TEXT_RSC_Variables.csv");
}

void MainWindow::CmdExportQuests() {
    if (!m_questsLoaded) return;

    auto folder = winutil::PickFolder(m_hwnd, L"Select export folder");
    if (!folder) return;

    std::string out;
    out.reserve(256 * 1024);

    csv::AppendRow(out, {
        "base_name",
        "display_name",
        "display_source_record",
        "guild_name",
        "qbn_path",
        "qrc_path",
        "guild_code",
        "membership_code",
        "membership",
        "min_rep",
        "child_guard",
        "delivery",
        "delivery_method",
        "state_count",
        "textvar_count",
        "opcode_count"
    });

    for (size_t i = 0; i < m_quests.quests.size(); ++i) {
        // Ensure we have QRC-derived display name when possible.
        m_quests.EnsureQrcLoaded(i, nullptr);
        const auto& q = m_quests.quests[i];

        std::string gc(1, q.guildCode ? q.guildCode : '?');
        std::string mc2(1, q.membershipCode ? q.membershipCode : '?');
        std::string rep(1, q.minRepCode ? q.minRepCode : '?');
        std::string cg(1, q.childGuardCode ? q.childGuardCode : '?');
        std::string del(1, q.deliveryCode ? q.deliveryCode : '?');

        char srcbuf[16]{};
        snprintf(srcbuf, sizeof(srcbuf), "0x%04X", (unsigned)q.displayNameSourceRecord);

        csv::AppendRow(out, {
            q.baseName,
            q.displayName,
            srcbuf,
            q.guildName,
            winutil::NarrowUtf8(q.qbnPath.wstring()),
            winutil::NarrowUtf8(q.qrcPath.wstring()),
            gc, mc2, q.membershipName, rep, cg, del, q.deliveryName,
            std::to_string(q.qbn.states.size()),
            std::to_string(q.qbn.textVars.size()),
            std::to_string(q.qbn.opcodes.size())
        });
    }

    std::wstring err;
    auto path = *folder / "QUESTS_List.csv";
    if (!csv::WriteUtf8File(path, out, &err)) {
        MessageBoxW(m_hwnd, err.c_str(), L"Export failed", MB_OK | MB_ICONERROR);
        return;
    }
    SetStatus(L"Exported QUESTS_List.csv");
}

void MainWindow::CmdExportQuestStages() {
    if (!m_questsLoaded) return;

    auto folder = winutil::PickFolder(m_hwnd, L"Select export folder");
    if (!folder) return;

    std::string out;
    out.reserve(512 * 1024);
    csv::AppendRow(out, { "base_name", "guild_name", "flag_index", "is_global", "global_index", "text_var_hash", "var_primary", "var_names" });

    auto hex32 = [](uint32_t v) {
        char b[16]{};
        snprintf(b, sizeof(b), "0x%08X", (unsigned)v);
        return std::string(b);
    };

    for (const auto& q : m_quests.quests) {
        for (const auto& s : q.qbn.states) {
            std::string names;
            for (size_t i = 0; i < s.varNames.size(); ++i) {
                if (i) names += "|";
                names += "_" + s.varNames[i] + "_";
            }

            std::string hv = hex32(s.textVarHash);

            std::string primary;
            if (!s.varNames.empty()) primary = "_" + s.varNames[0] + "_";

            csv::AppendRow(out, {
                q.baseName,
                q.guildName,
                std::to_string((int)s.flagIndex),
                s.isGlobal ? "1" : "0",
                std::to_string((int)s.globalIndex),
                hv,
                primary,
                names
            });
        }
    }

    std::wstring err;
    auto path = *folder / "QUESTS_Stages.csv";
    if (!csv::WriteUtf8File(path, out, &err)) {
        MessageBoxW(m_hwnd, err.c_str(), L"Export failed", MB_OK | MB_ICONERROR);
        return;
    }
    SetStatus(L"Exported QUESTS_Stages.csv");
}


void MainWindow::CmdExportTes4QuestDialogue() {
    if (!m_questsLoaded) return;

    auto folder = winutil::PickFolder(m_hwnd, L"Select export folder");
    if (!folder) return;

    auto Sanitize = [](std::string s) {
        // Normalize line endings and strip NULs; keep UTF-8.
        s.erase(std::remove(s.begin(), s.end(), '\0'), s.end());
        for (char& c : s) {
            if (c == '\r') c = '\n';
        }
        // Collapse excessive whitespace without destroying readability.
        // Avoid <regex> dependency; reduce runs of 3+ newlines to 2.
        size_t pos = 0;
        while ((pos = s.find("\n\n\n")) != std::string::npos) {
            s.erase(pos, 1);
        }
        return s;
    };

    auto EscapeField = [](const std::string& s) {
        // Pipe-delimited writer; escape \, |, and newlines.
        std::string o;
        o.reserve(s.size() + 16);
        for (char c : s) {
            if (c == '\\') o += "\\\\";
            else if (c == '|') o += "\\|";
            else if (c == '\n') o += "\\n";
            else if ((unsigned char)c < 0x20) { /* drop */ }
            else o.push_back(c);
        }
        return o;
    };

    auto RecordPlain = [&](arena2::TextRsc& rsc, uint16_t recId) -> std::string {
        auto* rec = rsc.FindMutable(recId);
        if (!rec) return {};
        rec->EnsureParsed(rsc.fileBytes);
        std::string out;
        for (size_t si = 0; si < rec->subrecords.size(); ++si) {
            auto& sr = rec->subrecords[si];
            auto& tok = sr.EnsureTokens();
            if (!tok.plain.empty()) {
                if (!out.empty()) out.append("\n");
                out.append(tok.plain);
            }
        }
        return Sanitize(out);
    };

    std::string out;
    out.reserve(2 * 1024 * 1024);

    out.append("# Daggerfall-CS -> TES4 import feed (QUST/DIAL/INFO)\n");
    out.append("# Format: pipe-delimited with backslash escapes.\\n is newline, \\| is literal pipe.\n");
    out.append("# Lines:\n");
    out.append("#   QUEST|<questEdid>|<questName>\n");
    out.append("#   STAGE|<questEdid>|<stage>|<logText>\n");
    out.append("#   TOPIC|<topicEdid>|<topicText>|<questEdid>\n");
    out.append("#   INFO|<topicEdid>|<responseText>|<condition>|<resultScript>\n");
    out.append("#\n");

    for (size_t qi = 0; qi < m_quests.quests.size(); ++qi) {
        m_quests.EnsureQrcLoaded(qi, nullptr);
        const auto& q = m_quests.quests[qi];

        std::string questEdid = "dfQUST_" + q.baseName;
        std::string questName = q.displayName.empty() ? q.baseName : q.displayName;

        out.append("QUEST|").append(EscapeField(questEdid)).append("|").append(EscapeField(questName)).append("\n");

        // Build stage map from opcode message IDs (deterministic encounter order).
        std::unordered_map<uint16_t, int> msgToStage;
        int nextStage = 10;

        auto stageForMsg = [&](uint16_t msgId) -> int {
            auto it = msgToStage.find(msgId);
            if (it != msgToStage.end()) return it->second;
            int v = nextStage;
            nextStage += 10;
            msgToStage[msgId] = v;
            return v;
        };

        // Create a single topic per quest as a CS-visible container.
        std::string topicEdid = "dfDIAL_" + q.baseName;
        std::string topicText = "Quest: " + questName;
        out.append("TOPIC|").append(EscapeField(topicEdid)).append("|").append(EscapeField(topicText)).append("|")
            .append(EscapeField(questEdid)).append("\n");

        arena2::TextRsc* qrc = q.qrcLoaded ? const_cast<arena2::TextRsc*>(&q.qrc) : nullptr;

        // Emit stages + INFO lines for each opcode that references a message.
        for (const auto& op : q.qbn.opcodes) {
            if (op.messageId == 0xFFFF) continue;

            const int stage = stageForMsg(op.messageId);
            std::string msg = qrc ? RecordPlain(*qrc, op.messageId) : std::string();
            if (msg.empty()) {
                char buf[32]{};
                snprintf(buf, sizeof(buf), "QRC 0x%04X", (unsigned)op.messageId);
                msg = buf;
            }

            // Stage log.
            out.append("STAGE|").append(EscapeField(questEdid)).append("|").append(std::to_string(stage)).append("|")
                .append(EscapeField(msg)).append("\n");

            // INFO response gated by quest stage.
            std::string cond = "GetStage " + questEdid + " >= " + std::to_string(stage);

            // Best-effort result script: advance stage to next known stage when player sees this.
            // Importer may ignore or refine.
            std::string result;
            // If we have more than one stage, drive linear progression.
            // Determine next stage in sequence.
            // (We compute later; for now keep empty and let importer decide.)
            result = "";

            out.append("INFO|").append(EscapeField(topicEdid)).append("|").append(EscapeField(msg)).append("|")
                .append(EscapeField(cond)).append("|").append(EscapeField(result)).append("\n");
        }

        // Emit a diagnostic SCRIPT block as pseudo-lines via INFO with empty conditions if desired.
        // We keep this out of the primary feed to avoid polluting CS dialogue unless explicitly consumed by the importer.
        // (Importer can choose to attach Disasm to QUST script instead.)
    }

    std::wstring err;
    auto path = *folder / "TES4_QuestDialogue.txt";
    if (!csv::WriteUtf8File(path, out, &err)) {
        MessageBoxW(m_hwnd, err.c_str(), L"Export failed", MB_OK | MB_ICONERROR);
        return;
    }

    SetStatus(L"Exported TES4_QuestDialogue.txt");
}
void MainWindow::OnLoadDone(LoadResult* r) {
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));

    HMENU hMenu = GetMenu(m_hwnd);
    EnableMenuItem(hMenu, IDM_FILE_OPEN_ARENA2, MF_BYCOMMAND | MF_ENABLED);
    EnableMenuItem(hMenu, IDM_FILE_OPEN_SPIRE, MF_BYCOMMAND | MF_ENABLED);
    DrawMenuBar(m_hwnd);

    if (!r->ok) {
        m_loading.store(false);
        MessageBoxW(m_hwnd, r->err.c_str(), L"Load failed", MB_OK | MB_ICONERROR);
        delete r;
        return;
    }

    m_text = std::move(r->text);
    m_quests = std::move(r->quests);
    m_questsLoaded = r->questsOk;
    m_bsaArchives = std::move(r->bsaArchives);
    m_bsaLoaded = r->bsaOk;

    PopulateTree();

    EnableMenuItem(hMenu, IDM_EXPORT_SUBRECORDS, MF_BYCOMMAND | MF_ENABLED);
    EnableMenuItem(hMenu, IDM_EXPORT_TOKENS, MF_BYCOMMAND | MF_ENABLED);
    EnableMenuItem(hMenu, IDM_EXPORT_VARIABLES, MF_BYCOMMAND | MF_ENABLED);
        EnableMenuItem(hMenu, IDM_EXPORT_QUESTS, MF_BYCOMMAND | (m_questsLoaded ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(hMenu, IDM_EXPORT_QUEST_STAGES, MF_BYCOMMAND | (m_questsLoaded ? MF_ENABLED : MF_GRAYED));
    DrawMenuBar(m_hwnd);

    wchar_t buf[512]{};
    swprintf_s(buf, L"Loaded %zu records from %s (BSA archives: %zu)", m_text.records.size(), m_text.sourcePath.wstring().c_str(), m_bsaArchives.size());
    SetStatus(buf);

    m_loading.store(false);
    delete r;
}

static std::wstring Widen(const std::string& s) { return winutil::WidenUtf8(s); }
static std::string Narrow(const std::wstring& s) { return winutil::NarrowUtf8(s); }

static std::string TrimAscii(std::string s) {
    auto issp = [](unsigned char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; };
    while (!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && issp((unsigned char)s.back())) s.pop_back();
    return s;
}

static std::vector<std::string> ParseCsvLine(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    bool inq = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (inq) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') { cur.push_back('"'); i++; }
                else inq = false;
            } else {
                cur.push_back(c);
            }
        } else {
            if (c == '"') inq = true;
            else if (c == ',') { out.push_back(cur); cur.clear(); }
            else cur.push_back(c);
        }
    }
    out.push_back(cur);
    for (auto& s : out) s = TrimAscii(s);
    return out;
}

void MainWindow::InitIndicesModel() {
    m_indicesRows.clear();
    m_varOverrides.clear();
    m_varImplemented.clear();

    std::filesystem::path exeDir = winutil::GetExeDirectory();
    std::filesystem::path p1 = exeDir / L"IndicesMacros.csv";
    std::filesystem::path p2 = exeDir / L"data" / L"IndicesMacros.csv";
    std::filesystem::path path;
    if (std::filesystem::exists(p1)) path = p1;
    else if (std::filesystem::exists(p2)) path = p2;

    if (!path.empty()) {
        std::ifstream f(path, std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

        size_t pos = 0;
        bool first = true;
        while (pos < bytes.size()) {
            size_t e = bytes.find('\n', pos);
            std::string line = (e == std::string::npos) ? bytes.substr(pos) : bytes.substr(pos, e - pos);
            pos = (e == std::string::npos) ? bytes.size() : (e + 1);

            line = TrimAscii(line);
            if (line.empty()) continue;

            if (first) { first = false; continue; } // header

            auto cols = ParseCsvLine(line);
            if (cols.size() < 5) continue;

            IndicesRow r{};
            r.token = cols[0];
            r.handler = cols[1];
            r.macroType = cols[2];
            r.implemented = (_stricmp(cols[3].c_str(), "yes") == 0);
            r.comment = cols[4];

            r.tokenW = Widen(r.token);

            std::string desc = r.comment;
            if (desc.empty()) desc = r.handler;
            r.descW = Widen(desc);

            r.handlerW = Widen(r.handler.empty() ? "null" : r.handler);
            r.typeW = Widen(r.macroType.empty() ? "singleline" : r.macroType);
            r.implW = Widen(r.implemented ? "yes" : "no");
            r.commentW = Widen(r.comment);
            r.valueW = L"";

            m_indicesRows.push_back(std::move(r));
        }
    }

    if (m_indicesRows.empty()) {
        auto add = [&](const char* tok, const char* handler, const char* type, bool impl, const char* comment) {
            IndicesRow r{};
            r.token = tok;
            r.handler = handler;
            r.macroType = type;
            r.implemented = impl;
            r.comment = comment;
            r.tokenW = Widen(r.token);
            r.descW = Widen(r.comment);
            r.handlerW = Widen(r.handler);
            r.typeW = Widen(r.macroType);
            r.implW = Widen(r.implemented ? "yes" : "no");
            r.commentW = Widen(r.comment);
            r.valueW = L"";
            m_indicesRows.push_back(std::move(r));
        };

        add("%pcn", "PlayerName", "singleline", true, "Character's full name");
        add("%ra", "PlayerRace", "singleline", true, "Player's race");
        add("%cn", "CityName", "singleline", true, "City name");
    }

    for (const auto& r : m_indicesRows) {
        m_varOverrides.emplace(r.token, std::string{});
        m_varImplemented[r.token] = r.implemented;
    }
}

std::string MainWindow::ApplyOverrides(std::string_view in) const {
    std::string out;
    out.reserve(in.size());

    auto isVarChar = [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    };

    for (size_t i = 0; i < in.size(); ++i) {
        unsigned char c = (unsigned char)in[i];

        if (c == '%') {
            size_t j = i + 1;
            if (j < in.size() && isVarChar((unsigned char)in[j])) {
                while (j < in.size() && isVarChar((unsigned char)in[j])) j++;
                std::string tok(in.substr(i, j - i));
                auto itImpl = m_varImplemented.find(tok);
                if (itImpl != m_varImplemented.end() && itImpl->second) {
                    auto it = m_varOverrides.find(tok);
                    if (it != m_varOverrides.end() && !it->second.empty()) {
                        out.append(it->second);
                        i = j - 1;
                        continue;
                    }
                }
            }
        }

        if (c == '_') {
            size_t j = i + 1;
            if (j < in.size() && isVarChar((unsigned char)in[j])) {
                while (j < in.size() && isVarChar((unsigned char)in[j])) j++;
                if (j < in.size() && in[j] == '_') {
                    std::string tok(in.substr(i, (j + 1) - i));
                    auto itImpl = m_varImplemented.find(tok);
                    if (itImpl != m_varImplemented.end() && itImpl->second) {
                        auto it = m_varOverrides.find(tok);
                        if (it != m_varOverrides.end() && !it->second.empty()) {
                            out.append(it->second);
                            i = j;
                            continue;
                        }
                    }
                }
            }
        }

        out.push_back((char)c);
    }

    return out;
}

void MainWindow::NoteDiscoveredVars(const arena2::TokenizedText& tt) {
    bool changed = false;

    auto hasToken = [&](const std::string& tok) {
        for (const auto& r : m_indicesRows) if (r.token == tok) return true;
        return false;
    };

    for (const auto& v : tt.vars) {
        const std::string tok = v.token;
        if (!hasToken(tok)) {
            IndicesRow r{};
            r.token = tok;
            r.handler = "UserOverride";
            r.macroType = "singleline";
            r.implemented = true;
            r.comment = "Discovered macro/variable (no catalog entry yet).";

            r.tokenW = Widen(r.token);
            r.descW = Widen(r.comment);
            r.handlerW = Widen(r.handler);
            r.typeW = Widen(r.macroType);
            r.implW = Widen("yes");
            r.commentW = Widen(r.comment);
            r.valueW = L"";

            m_indicesRows.push_back(std::move(r));
            m_varOverrides.emplace(tok, std::string{});
            m_varImplemented[tok] = true;
            changed = true;
        }
    }

    if (changed && m_indicesWnd.IsOpen()) {
        m_indicesWnd.Refresh();
    }
}

void MainWindow::SetIndexOverrideByRow(int row, const std::wstring& value) {
    if (row < 0 || row >= (int)m_indicesRows.size()) return;
    auto& r = m_indicesRows[row];
    if (!r.implemented) return;

    r.valueW = value;
    m_varOverrides[r.token] = Narrow(value);

    RefreshPreviewIfVisible();
    SaveIndicesOverrides();
}

void MainWindow::RefreshPreviewIfVisible() {
    if (m_viewMode == ViewMode::TextSubrecords) {
        auto* p = GetSelectedPayload();
        if (!p || p->kind != TreePayload::Kind::TextRecord) return;

        auto* rec = m_text.FindMutable(p->textRecordId);
        if (!rec) return;

        int sel = ListView_GetNextItem(m_list, -1, LVNI_SELECTED);
        if (sel < 0) sel = 0;
        ShowSubrecordPreview(*rec, sel);
        return;
    }

    if (m_viewMode == ViewMode::QuestStages) {
        if (m_activeQuest == (size_t)-1) return;
        int sel = ListView_GetNextItem(m_list, -1, LVNI_SELECTED);
        if (sel < 0) sel = 0;
        ShowQuestStagePreview(m_activeQuest, sel);
        return;
    }

    if (m_viewMode == ViewMode::QuestText) {
        if (m_activeQuest == (size_t)-1) return;
        int sel = ListView_GetNextItem(m_list, -1, LVNI_SELECTED);
        if (sel < 0) sel = 0;
        ShowQuestTextPreview(m_activeQuest, sel);
        return;
    }
}

void MainWindow::LoadIndicesOverrides() {
    std::filesystem::path ini = winutil::GetExeDirectory() / L"DaggerfallCS.ini";

    for (auto& r : m_indicesRows) {
        wchar_t buf[8192]{};
        GetPrivateProfileStringW(L"Indices", r.tokenW.c_str(), L"", buf, 8192, ini.wstring().c_str());
        r.valueW = buf;
        m_varOverrides[r.token] = Narrow(r.valueW);
        m_varImplemented[r.token] = r.implemented;
    }
}

void MainWindow::SaveIndicesOverrides() {
    std::filesystem::path ini = winutil::GetExeDirectory() / L"DaggerfallCS.ini";

    for (const auto& r : m_indicesRows) {
        WritePrivateProfileStringW(L"Indices", r.tokenW.c_str(), r.valueW.c_str(), ini.wstring().c_str());
    }
}


ui::MainWindow::TreePayload* MainWindow::AddPayload(TreePayload::Kind kind, uint16_t recId, size_t questIdx, size_t bsaArchiveIdx, size_t bsaEntryIdx) {
    auto p = std::make_unique<TreePayload>();
    p->kind = kind;
    p->textRecordId = recId;
    p->questIndex = questIdx;
    p->bsaArchiveIndex = bsaArchiveIdx;
    p->bsaEntryIndex = bsaEntryIdx;
    TreePayload* raw = p.get();
    m_treePayloads.push_back(std::move(p));
    return raw;
}

ui::MainWindow::TreePayload* MainWindow::GetSelectedPayload() const {
    TVITEMW it{};
    it.mask = TVIF_PARAM | TVIF_HANDLE;
    it.hItem = TreeView_GetSelection(m_tree);
    if (!it.hItem) return nullptr;
    TreeView_GetItem(m_tree, &it);
    return reinterpret_cast<TreePayload*>(it.lParam);
}

void MainWindow::SetupListColumns_TextSubrecords() {
    ListView_DeleteAllItems(m_list);
    while (ListView_DeleteColumn(m_list, 0)) {}
    LVCOLUMNW c{};
    c.mask = LVCF_TEXT | LVCF_WIDTH;
    c.cx = 110;
    c.pszText = const_cast<wchar_t*>(L"Subrecord");
    ListView_InsertColumn(m_list, 0, &c);
    c.cx = 520;
    c.pszText = const_cast<wchar_t*>(L"Preview");
    ListView_InsertColumn(m_list, 1, &c);
    m_viewMode = ViewMode::TextSubrecords;
}

void MainWindow::SetupListColumns_BsaDialogueScript() {
    ListView_DeleteAllItems(m_list);
    while (ListView_DeleteColumn(m_list, 0)) {}

    LVCOLUMNW c{};
    c.mask = LVCF_TEXT | LVCF_WIDTH;
    c.cx = 120;
    c.pszText = const_cast<wchar_t*>(L"Saycode");
    ListView_InsertColumn(m_list, 0, &c);

    c.cx = 560;
    c.pszText = const_cast<wchar_t*>(L"NPC SAY");
    ListView_InsertColumn(m_list, 1, &c);

    c.cx = 160;
    c.pszText = const_cast<wchar_t*>(L"Replycode");
    ListView_InsertColumn(m_list, 2, &c);

    m_viewMode = ViewMode::BsaEntry;
}

void MainWindow::SetupListColumns_BsaDialogueRouting() {
    ListView_DeleteAllItems(m_list);
    while (ListView_DeleteColumn(m_list, 0)) {}

    LVCOLUMNW c{};
    c.mask = LVCF_TEXT | LVCF_WIDTH;
    c.cx = 160;
    c.pszText = const_cast<wchar_t*>(L"Rule / Range");
    ListView_InsertColumn(m_list, 0, &c);

    c.cx = 220;
    c.pszText = const_cast<wchar_t*>(L"TalkGroup / Target");
    ListView_InsertColumn(m_list, 1, &c);

    c.cx = 340;
    c.pszText = const_cast<wchar_t*>(L"Notes");
    ListView_InsertColumn(m_list, 2, &c);

    m_viewMode = ViewMode::BsaEntry;
}

void MainWindow::SetupListColumns_BsaTable(const std::vector<std::wstring>& headers) {
    ListView_DeleteAllItems(m_list);
    while (ListView_DeleteColumn(m_list, 0)) {}

    LVCOLUMNW c{};
    c.mask = LVCF_TEXT | LVCF_WIDTH;
    for (size_t i = 0; i < headers.size(); ++i) {
        c.cx = (i == 0) ? 140 : 220;
        c.pszText = const_cast<wchar_t*>(headers[i].c_str());
        ListView_InsertColumn(m_list, (int)i, &c);
    }

    m_viewMode = ViewMode::BsaEntry;
}

void MainWindow::OnBsaDialogueListContextMenu(LPARAM lParam) {
    if (m_viewMode != ViewMode::BsaEntry) return;

    POINT pt{};
    pt.x = GET_X_LPARAM(lParam);
    pt.y = GET_Y_LPARAM(lParam);
    if (pt.x == -1 && pt.y == -1) {
        int row = ListView_GetNextItem(m_list, -1, LVNI_SELECTED);
        if (row < 0) row = 0;
        RECT rc{};
        if (!ListView_GetSubItemRect(m_list, row, 0, LVIR_BOUNDS, &rc)) return;
        pt.x = rc.left + 8;
        pt.y = rc.top + 8;
        ClientToScreen(m_list, &pt);
    }

    POINT client = pt;
    ScreenToClient(m_list, &client);

    LVHITTESTINFO hit{};
    hit.pt = client;
    int row = ListView_SubItemHitTest(m_list, &hit);

    FlcSpeakTarget target{};
    const bool hasDialogueSpeak = (m_bsaDialogueKind == BsaDialogueKind::Script && row >= 0 && hit.iSubItem >= 0 &&
        BuildFlcSpeakTargetFromListClick(row, hit.iSubItem, target));

    auto* p = GetSelectedPayload();
    bool hasEntrySpeak = false;
    if (p && p->kind == TreePayload::Kind::BsaEntry && p->bsaArchiveIndex < m_bsaArchives.size()) {
        const auto& ar = m_bsaArchives[p->bsaArchiveIndex];
        if (_wcsicmp(ar.sourcePath.filename().wstring().c_str(), L"FLC.BSA") == 0 && p->bsaEntryIndex < ar.entries.size()) {
            std::wstring name = ToLowerWs(winutil::WidenUtf8(ar.entries[p->bsaEntryIndex].name));
            hasEntrySpeak = EndsWithWs(name, L".flc");
        }
    }

    if (!hasDialogueSpeak && !hasEntrySpeak) return;

    HMENU h = CreatePopupMenu();
    if (!h) return;

    if (hasDialogueSpeak) {
        std::wstring label = L"Speak";
        if (!target.code.empty()) label += L" (" + target.code + L")";
        AppendMenuW(h, MF_STRING, IDM_BSA_DIALOGUE_SPEAK, label.c_str());
    }
    if (hasEntrySpeak) {
        AppendMenuW(h, MF_STRING, IDM_BSA_ENTRY_SPEAK, L"Speak file audio");
    }

    SetForegroundWindow(m_hwnd);
    UINT cmd = TrackPopupMenu(h, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, nullptr);
    DestroyMenu(h);

    if (cmd == IDM_BSA_DIALOGUE_SPEAK) {
        CmdSpeakBsaDialogueLine(target);
    } else if (cmd == IDM_BSA_ENTRY_SPEAK) {
        CmdSpeakSelectedFlcEntry();
    }
}

bool MainWindow::BuildFlcSpeakTargetFromListClick(int row, int subItem, FlcSpeakTarget& out) const {
    if (row < 0 || (size_t)row >= m_bsaDialogueRows.size()) return false;
    if (subItem != 1 && subItem != 2) return false; // NPC SAY or Replycode columns

    const auto& src = m_bsaDialogueRows[(size_t)row];

    auto getCell = [&](size_t idx) -> std::wstring {
        return idx < src.size() ? TrimWs(src[idx]) : L"";
    };

    const bool reply = (subItem == 2);
    std::wstring code = reply ? getCell(2) : getCell(0);
    std::wstring text = reply ? getCell(5) : getCell(1);

    if (code.empty()) {
        for (int i = row - 1; i >= 0; --i) {
            const auto& prev = m_bsaDialogueRows[(size_t)i];
            size_t idx = reply ? 2u : 0u;
            if (idx < prev.size()) {
                std::wstring c = TrimWs(prev[idx]);
                if (!c.empty()) { code = std::move(c); break; }
            }
        }
    }

    if (code.empty() || _wcsicmp(code.c_str(), L"END") == 0) return false;
    if (text.empty()) text = L"(line text unavailable)";

    out.code = std::move(code);
    out.lineText = std::move(text);
    out.replySide = reply;
    return true;
}

void MainWindow::CmdSpeakBsaDialogueLine(const FlcSpeakTarget& target) {
    if (target.code.empty()) return;

    const battlespire::BsaArchive* flcBsa = nullptr;
    for (const auto& a : m_bsaArchives) {
        if (_wcsicmp(a.sourcePath.filename().wstring().c_str(), L"FLC.BSA") == 0) {
            flcBsa = &a;
            break;
        }
    }

    if (!flcBsa) {
        MessageBoxW(m_hwnd, L"FLC.BSA is not loaded under BSA Archives.", L"Speak", MB_OK | MB_ICONWARNING);
        return;
    }

    DialogueCodeParts parts{};
    if (!TryParseDialogueCodeParts(target.code, parts)) {
        MessageBoxW(m_hwnd, (L"Speak error: Could not parse dialogue code: " + target.code).c_str(), L"Speak error", MB_OK | MB_ICONWARNING);
        return;
    }

    const std::wstring mode = target.replySide ? L"WAIT" : L"TALK";

    struct Candidate { const battlespire::BsaEntry* e{}; int variant{}; std::wstring name; };
    std::vector<Candidate> cands;
    for (const auto& e : flcBsa->entries) {
        std::wstring spk, emode;
        int v = -1;
        if (!TryParseFlcVariant(winutil::WidenUtf8(e.name), spk, emode, v)) continue;
        if (spk != parts.speaker) continue;
        if (emode != mode) continue;
        cands.push_back(Candidate{ &e, v, winutil::WidenUtf8(e.name) });
    }

    if (cands.empty()) {
        std::wstring msg = L"No matching " + mode + L" animation clips found in FLC.BSA for speaker code " + parts.speaker + L".";
        MessageBoxW(m_hwnd, msg.c_str(), L"Speak", MB_OK | MB_ICONWARNING);
        return;
    }

    std::stable_sort(cands.begin(), cands.end(), [](const Candidate& a, const Candidate& b) {
        if (a.variant != b.variant) return a.variant < b.variant;
        return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
    });

    int wanted = parts.terminalNumber % 100;
    const Candidate* picked = nullptr;
    for (const auto& c : cands) {
        if (c.variant == wanted) { picked = &c; break; }
    }
    if (!picked) {
        picked = &cands[(size_t)(parts.terminalNumber >= 0 ? (parts.terminalNumber % (int)cands.size()) : 0)];
    }

    std::vector<uint8_t> bytes;
    std::wstring err;
    if (!flcBsa->ReadEntryData(*picked->e, bytes, &err)) {
        MessageBoxW(m_hwnd, (L"Failed to read FLC clip: " + err).c_str(), L"Speak", MB_OK | MB_ICONERROR);
        return;
    }

    bool stripped = false;
    battlespire::FlcFile::NormalizeLeadingPrefix(bytes, stripped);

    auto tempDir = std::filesystem::temp_directory_path();
    std::filesystem::path out = tempDir / (L"DaggerfallCS_" + parts.speaker + L"_" + mode + L"_" + std::to_wstring(picked->variant) + L".flc");

    std::ofstream f(out, std::ios::binary);
    if (!f) {
        MessageBoxW(m_hwnd, L"Failed to write temporary FLC file.", L"Speak", MB_OK | MB_ICONERROR);
        return;
    }
    if (!bytes.empty()) f.write(reinterpret_cast<const char*>(bytes.data()), (std::streamsize)bytes.size());
    f.close();

    HINSTANCE h = ShellExecuteW(m_hwnd, L"open", out.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32) {
        std::wstring msg = L"Could not open FLC clip in the default viewer. File saved to:\r\n" + out.wstring();
        MessageBoxW(m_hwnd, msg.c_str(), L"Speak", MB_OK | MB_ICONWARNING);
        return;
    }

    std::wstring where = target.replySide ? L"Reply" : L"NPC SAY";
    std::wstring st = L"Speak " + where + L": " + target.code + L" -> " + picked->name;
    if (stripped) st += L" (normalized)";
    SetStatus(st);
}


void MainWindow::CmdSpeakSelectedFlcEntry() {
    auto* p = GetSelectedPayload();
    if (!p || p->kind != TreePayload::Kind::BsaEntry || p->bsaArchiveIndex >= m_bsaArchives.size()) return;

    const auto& ar = m_bsaArchives[p->bsaArchiveIndex];
    if (_wcsicmp(ar.sourcePath.filename().wstring().c_str(), L"FLC.BSA") != 0 || p->bsaEntryIndex >= ar.entries.size()) return;

    const auto& e = ar.entries[p->bsaEntryIndex];
    std::wstring entryName = winutil::WidenUtf8(e.name);
    std::wstring lowerName = ToLowerWs(entryName);
    if (!EndsWithWs(lowerName, L".flc")) {
        MessageBoxW(m_hwnd, L"Selected entry is not an FLC clip.", L"Speak", MB_OK | MB_ICONWARNING);
        return;
    }

    std::vector<uint8_t> bytes;
    std::wstring err;
    if (!ar.ReadEntryData(e, bytes, &err)) {
        MessageBoxW(m_hwnd, (L"Failed to read FLC clip: " + err).c_str(), L"Speak", MB_OK | MB_ICONERROR);
        return;
    }

    bool stripped = false;
    battlespire::FlcFile::NormalizeLeadingPrefix(bytes, stripped);

    std::wstring safeName = BaseNameOnly(entryName);
    for (auto& c : safeName) {
        if (c == L'/' || c == L'\\' || c == L':' || c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|') c = L'_';
    }

    std::filesystem::path out = std::filesystem::temp_directory_path() / (L"DaggerfallCS_" + safeName);
    std::ofstream f(out, std::ios::binary);
    if (!f) {
        MessageBoxW(m_hwnd, L"Failed to write temporary FLC file.", L"Speak", MB_OK | MB_ICONERROR);
        return;
    }
    if (!bytes.empty()) f.write(reinterpret_cast<const char*>(bytes.data()), (std::streamsize)bytes.size());
    f.close();

    HINSTANCE h = ShellExecuteW(m_hwnd, L"open", out.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32) {
        std::wstring msg = L"Could not open FLC clip in the default viewer. File saved to:\r\n" + out.wstring();
        MessageBoxW(m_hwnd, msg.c_str(), L"Speak", MB_OK | MB_ICONWARNING);
        return;
    }

    std::wstring st = L"Speak file: " + entryName;
    if (stripped) st += L" (normalized)";
    SetStatus(st);
}
void MainWindow::ShowBsaDialogueRowPreview(int row) {
    if (row < 0 || (size_t)row >= m_bsaDialogueRows.size()) return;

    const auto& src = m_bsaDialogueRows[(size_t)row];
    std::wstring out;
    out.reserve(1024);

    if (!m_bsaDialogueTitle.empty()) {
        out += L"Entry: " + m_bsaDialogueTitle + L"\r\n\r\n";
    }

    for (size_t i = 0; i < src.size(); ++i) {
        std::wstring key;
        if (i < m_bsaDialogueHeaders.size() && !TrimWs(m_bsaDialogueHeaders[i]).empty()) key = m_bsaDialogueHeaders[i];
        else key = L"Col" + std::to_wstring(i + 1);

        out += key;
        out += L": ";
        out += src[i];
        out += L"\r\n";
    }

    SetWindowTextW(m_preview, out.c_str());
}

void MainWindow::SetupListColumns_QuestStages() {
    ListView_DeleteAllItems(m_list);
    while (ListView_DeleteColumn(m_list, 0)) {}

    LVCOLUMNW c{};
    c.mask = LVCF_TEXT | LVCF_WIDTH;

    c.cx = 60;
    c.pszText = const_cast<wchar_t*>(L"Stage");
    ListView_InsertColumn(m_list, 0, &c);

    c.cx = 360;
    c.pszText = const_cast<wchar_t*>(L"Log Entry");
    ListView_InsertColumn(m_list, 1, &c);

    c.cx = 260;
    c.pszText = const_cast<wchar_t*>(L"Script");
    ListView_InsertColumn(m_list, 2, &c);

    c.cx = 220;
    c.pszText = const_cast<wchar_t*>(L"Variable");
    ListView_InsertColumn(m_list, 3, &c);

    c.cx = 90;
    c.pszText = const_cast<wchar_t*>(L"Scope");
    ListView_InsertColumn(m_list, 4, &c);

    m_viewMode = ViewMode::QuestStages;
}

void MainWindow::SetupListColumns_QuestText() {
    ListView_DeleteAllItems(m_list);
    while (ListView_DeleteColumn(m_list, 0)) {}

    LVCOLUMNW c{};
    c.mask = LVCF_TEXT | LVCF_WIDTH;

    c.cx = 90;
    c.pszText = const_cast<wchar_t*>(L"ID");
    ListView_InsertColumn(m_list, 0, &c);

    c.cx = 200;
    c.pszText = const_cast<wchar_t*>(L"Type");
    ListView_InsertColumn(m_list, 1, &c);

    c.cx = 420;
    c.pszText = const_cast<wchar_t*>(L"Preview");
    ListView_InsertColumn(m_list, 2, &c);

    m_viewMode = ViewMode::QuestText;
}


static const wchar_t* WellKnownQrcType(uint16_t rid) {
    switch (rid) {
        case 0x03E8: return L"Get Quest (Offer)";
        case 0x03E9: return L"Decline";
        case 0x03EA: return L"Accept";
        case 0x03EB: return L"Don't Bother Me";
        case 0x03EC: return L"Quest Complete";
        case 0x03ED: return L"Info/Rumours";
        case 0x03EE: return L"Rumours";
        case 0x03F0: return L"Info/Rumours";
        case 0x03F1: return L"Quest Failed (Guild)";
        case 0x03F2: return L"Quest Log Entry";
        case 0x03F3: return L"Quest Item Found (Body)";
        case 0x0410: return L"Quest Item Found (Ground)";
        case 0x0415: return L"Time Expired";
        default:     return L"(Text)";
    }
}
static std::wstring HexU16(uint16_t v) {
    wchar_t buf[16]{};
    swprintf_s(buf, L"0x%04X", (unsigned)v);
    return buf;
}

static std::wstring HexU32(uint32_t v) {
    wchar_t buf[24]{};
    swprintf_s(buf, L"0x%08X", (unsigned)v);
    return buf;
}

static const wchar_t* QbnSectionName(uint16_t sid) {
    switch (sid) {
        case 0:  return L"Item";
        case 1:  return L"Clock";
        case 2:  return L"Foe";
        case 3:  return L"NPC";
        case 4:  return L"Location";
        case 5:  return L"Person";
        case 6:  return L"Timer";
        case 7:  return L"Mob";
        case 8:  return L"QRC";
        case 9:  return L"State";
        case 10: return L"Task";
        default: return L"Section";
    }
}

static bool IsConstSubRecord(const arena2::QbnSubRecord& s) {
    // Quest hacking guide describes a constant sub-record flavor where the "long int" field is 0x12345678
    // and the section short is 0; the constant is stored in Value.
    return (s.sectionId == 0) && (s.localPtr == 0x12345678u);
}

static uint8_t SectionFromLocalPtr(uint32_t lp) { return (uint8_t)((lp >> 8) & 0xFFu); }
static std::wstring FormatSubRefCompact(const arena2::QbnSubRecord& s) {
    std::wstring out;

    if (s.notFlag) out += L"NOT ";

    if (IsConstSubRecord(s)) {
        out += L"Const(" + HexU32(s.value) + L")";
        return out;
    }

    uint16_t sid = s.sectionId ? s.sectionId : (uint16_t)SectionFromLocalPtr(s.localPtr);
    int rec = s.RecordIndex();
    if (rec == 0xFF) rec = -1;

    out += QbnSectionName(sid);
    out += L"[";
    out += (rec < 0) ? L"*" : std::to_wstring(rec);
    out += L"]";

    // Show literal values only when the record index is unknown (debug) or section is unknown.
    if ((sid == 0 || sid > 10) && s.value != 0xFFFFFFFFu && s.value != 0xFFFFFFFEu) {
        out += L" val=" + HexU32(s.value);
    }
    return out;
}

static std::wstring VarFromFirstName(const std::vector<std::string>& names) {
    if (names.empty()) return L"(unresolved)";
    return L"_" + winutil::WidenUtf8(names[0]) + L"_";
}

static std::wstring ResolveSectionRecordPretty(const arena2::QuestEntry& q, uint16_t sid, int rec) {
    if (rec < 0) return L"*";
    switch (sid) {
        case 0: { // Items
            if (auto* it = q.qbn.FindItem((uint16_t)rec)) return VarFromFirstName(it->varNames) + L" (Item#" + std::to_wstring(rec) + L")";
            return L"(Item#" + std::to_wstring(rec) + L")";
        }
        case 3: { // NPCs
            if (auto* n = q.qbn.FindNpc((uint16_t)rec)) return VarFromFirstName(n->varNames) + L" (NPC#" + std::to_wstring(rec) + L")";
            return L"(NPC#" + std::to_wstring(rec) + L")";
        }
        case 4: { // Locations
            if (auto* l = q.qbn.FindLocation((uint16_t)rec)) return VarFromFirstName(l->varNames) + L" (Loc#" + std::to_wstring(rec) + L")";
            return L"(Loc#" + std::to_wstring(rec) + L")";
        }
        case 6: { // Timers
            if (auto* t = q.qbn.FindTimer((uint16_t)rec)) return VarFromFirstName(t->varNames) + L" (Timer#" + std::to_wstring(rec) + L")";
            return L"(Timer#" + std::to_wstring(rec) + L")";
        }
        case 7: { // Mobs
            if (auto* m = q.qbn.FindMob((uint16_t)rec)) return VarFromFirstName(m->varNames) + L" (Mob#" + std::to_wstring(rec) + L")";
            return L"(Mob#" + std::to_wstring(rec) + L")";
        }
        case 9: { // States
            uint16_t stageNum = 0xFFFF;
            if (rec >= 0 && rec < (int)q.qbn.states.size()) stageNum = q.qbn.states[(size_t)rec].flagIndex;
            if (stageNum == 0xFFFF) stageNum = (uint16_t)rec;

            if (rec >= 0 && rec < (int)q.qbn.states.size())
                return VarFromFirstName(q.qbn.states[(size_t)rec].varNames) + L" (Stage " + std::to_wstring(stageNum) + L")";
            return L"(Stage " + std::to_wstring(stageNum) + L")";
        }
        default:
            return std::wstring(QbnSectionName(sid)) + L"#" + std::to_wstring(rec);
    }
}

static int SubRefIndexBest(const arena2::QbnSubRecord& s) {
    if (s.sectionId == 0) return -1;
    int rec = (int)(s.localPtr & 0xFFu);
    if (rec == 0xFF) {
        if (s.value != 0xFFFFFFFFu && s.value != 0xFFFFFFFEu) {
            rec = (int)(s.value & 0xFFFFu);
        }
    }
    if (rec == 0xFF) return -1;
    return rec;
}

static std::wstring FormatSubRefPretty(const arena2::QuestEntry& q, const arena2::QbnSubRecord& s) {
    std::wstring out;
    if (s.notFlag) out += L"NOT ";

    if (IsConstSubRecord(s)) {
        // If it looks like a message id, present it as such.
        if (s.value <= 0xFFFFu && s.value != 0xFFFFu) out += L"Msg(" + HexU16((uint16_t)s.value) + L")";
        else out += L"Const(" + HexU32(s.value) + L")";
        return out;
    }

    uint16_t sid = s.sectionId ? s.sectionId : (uint16_t)SectionFromLocalPtr(s.localPtr);
    int rec = SubRefIndexBest(s);
    out += ResolveSectionRecordPretty(q, sid, rec);
    return out;
}


// Whether Sub-record 1 (State) is a CONDITION gate for this op-code (vs being a "state to set").


void MainWindow::PopulateQuestStages(size_t questIdx)
{
    SetupListColumns_QuestStages();
    ListView_DeleteAllItems(m_list);
    SetWindowTextW(m_preview, L"");

    if (!m_questsLoaded || questIdx >= m_quests.quests.size())
        return;

    std::wstring err;
    if (!m_quests.EnsureQrcLoaded(questIdx, &err)) {
        std::wstring out = L"Failed to load QRC: " + err;
        SetWindowTextW(m_preview, out.c_str());
        return;
    }

    arena2::QuestEntry& q = m_quests.quests[questIdx];

    if (!q.qbnLoaded) {
        std::wstring qerr;
        q.qbnLoaded = q.qbn.LoadFromFile(q.qbnPath, m_quests.hashes, &qerr);
        if (!q.qbnLoaded) {
            std::wstring out = L"Failed to load QBN: " + qerr;
            SetWindowTextW(m_preview, out.c_str());
            return;
        }
    }

    const auto& qbn = q.qbn;
    const size_t nStates = qbn.states.size();

    auto trimOneLineW = [](std::wstring s, size_t maxLen) -> std::wstring {
        for (auto& ch : s) {
            if (ch == L'\r' || ch == L'\n' || ch == L'\t')
                ch = L' ';
        }
        while (!s.empty() && s.front() == L' ') s.erase(s.begin());
        while (!s.empty() && s.back() == L' ') s.pop_back();
        if (s.size() > maxLen) {
            s.resize(maxLen);
            s += L"...";
        }
        return s;
    };

    auto stateIdxFromSub = [&](const arena2::QbnSubRecord& sr) -> int {
        if (sr.sectionId != 9)
            return -1;
        int idx = SubRefIndexBest(sr);
        if (idx < 0)
            return -1;
        if ((size_t)idx >= nStates)
            return -1;
        return idx;
    };

    std::vector<uint32_t> gateCount(nStates, 0);
    std::vector<uint32_t> targetCount(nStates, 0);
    std::vector<uint32_t> anyCount(nStates, 0);
    std::vector<std::vector<uint16_t>> logMsgs(nStates);

	for (const auto& op : qbn.opcodes) {
        int gate = stateIdxFromSub(op.sub[0]);
        if (gate >= 0) {
            gateCount[gate]++;
            anyCount[gate]++;
        }

        for (int i = 1; i < 5; i++) {
            int t = stateIdxFromSub(op.sub[i]);
            if (t >= 0) {
                targetCount[t]++;
                anyCount[t]++;
            }
        }

        if (op.opCode == 0x0017u && gate >= 0 && op.messageId != 0x0000u && op.messageId != 0xFFFFu) {
            logMsgs[gate].push_back(op.messageId);
        }
    }

    auto qrcPlain = [&](uint16_t recId) -> std::wstring {
        if (!q.qrcLoaded)
            return L"";
        arena2::TextRecord* rec = q.qrc.FindMutable(recId);
        if (!rec)
            return L"";
        rec->EnsureParsed(q.qrc.fileBytes);
        if (rec->subrecords.empty())
            return L"";
        arena2::TextSubrecord& sr = rec->subrecords[0];
        arena2::TokenizedText& tt = sr.EnsureTokens();
        const std::string& base = sr.hasUserOverride ? sr.userOverride : tt.plain;
        std::string txt = ApplyOverrides(base);
        return trimOneLineW(winutil::WidenUtf8(txt), 120);
    };

    int row = 0;
    for (size_t s = 0; s < nStates; s++) {
        const auto& st = qbn.states[s];
        uint16_t stageNum = (st.flagIndex != 0xFFFFu) ? st.flagIndex : (uint16_t)s;

        std::wstring colStage = std::to_wstring(stageNum);

        std::wstring colLog;
        if (!logMsgs[s].empty())
            colLog = qrcPlain(logMsgs[s][0]);

        std::wstring colScript = L"G:" + std::to_wstring(gateCount[s]) + L" T:" + std::to_wstring(targetCount[s]);

		std::wstring colVar = VarFromFirstName(st.varNames);
        if (colVar.empty())
            colVar = HexU32(st.textVarHash);

        std::wstring colScope = st.isGlobal ? (L"Global(" + std::to_wstring(st.globalIndex) + L")") : L"Local";

        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = row;
        item.pszText = const_cast<LPWSTR>(colStage.c_str());
        item.lParam = (LPARAM)s;
        int at = ListView_InsertItem(m_list, &item);
        if (at >= 0) {
            ListView_SetItemText(m_list, at, 1, const_cast<LPWSTR>(colLog.c_str()));
            ListView_SetItemText(m_list, at, 2, const_cast<LPWSTR>(colScript.c_str()));
            ListView_SetItemText(m_list, at, 3, const_cast<LPWSTR>(colVar.c_str()));
            ListView_SetItemText(m_list, at, 4, const_cast<LPWSTR>(colScope.c_str()));
        }
        row++;
    }

    if (nStates > 0) {
        ListView_SetItemState(m_list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ShowQuestStagePreview(questIdx, 0);
    }
}



void MainWindow::ShowQuestStagePreview(size_t questIdx, int listIndex)
{
    if (!m_questsLoaded || questIdx >= m_quests.quests.size() || listIndex < 0) {
        SetWindowTextW(m_preview, L"");
        return;
    }

    arena2::QuestEntry& q = m_quests.quests[questIdx];

    if (!q.qbnLoaded) {
        std::wstring qerr;
        q.qbnLoaded = q.qbn.LoadFromFile(q.qbnPath, m_quests.hashes, &qerr);
        if (!q.qbnLoaded) {
            std::wstring out = L"Failed to load QBN: " + qerr;
            SetWindowTextW(m_preview, out.c_str());
            return;
        }
    }

    std::wstring err;
    m_quests.EnsureQrcLoaded(questIdx, &err);

    LVITEMW it{};
    it.mask = LVIF_PARAM;
    it.iItem = listIndex;
    it.iSubItem = 0;
    if (!ListView_GetItem(m_list, &it)) {
        SetWindowTextW(m_preview, L"");
        return;
    }

    const int stateIdx = (int)it.lParam;
    if (stateIdx < 0 || (size_t)stateIdx >= q.qbn.states.size()) {
        SetWindowTextW(m_preview, L"");
        return;
    }

    const auto& st = q.qbn.states[stateIdx];
    const uint16_t stageNum = (st.flagIndex != 0xFFFFu) ? st.flagIndex : (uint16_t)stateIdx;

    auto trimOneLineW = [](std::wstring s, size_t maxLen) -> std::wstring {
        for (auto& ch : s) {
            if (ch == L'\r' || ch == L'\n' || ch == L'\t')
                ch = L' ';
        }
        while (!s.empty() && s.front() == L' ') s.erase(s.begin());
        while (!s.empty() && s.back() == L' ') s.pop_back();
        if (s.size() > maxLen) {
            s.resize(maxLen);
            s += L"...";
        }
        return s;
    };

    auto qrcPlain = [&](uint16_t recId) -> std::wstring {
        if (!q.qrcLoaded)
            return L"";
        arena2::TextRecord* rec = q.qrc.FindMutable(recId);
        if (!rec)
            return L"";
        rec->EnsureParsed(q.qrc.fileBytes);
        if (rec->subrecords.empty())
            return L"";
        arena2::TextSubrecord& sr = rec->subrecords[0];
        arena2::TokenizedText& tt = sr.EnsureTokens();
        const std::string& base = sr.hasUserOverride ? sr.userOverride : tt.plain;
        std::string txt = ApplyOverrides(base);
        return trimOneLineW(winutil::WidenUtf8(txt), 160);
    };

    auto stateIdxFromSub = [&](const arena2::QbnSubRecord& sr) -> int {
        if (sr.sectionId != 9)
            return -1;
        return SubRefIndexBest(sr);
    };

    std::wstring varName;
	varName = VarFromFirstName(st.varNames);
    if (varName.empty())
        varName = HexU32(st.textVarHash);

    std::wstring scope = st.isGlobal ? (L"Global(" + std::to_wstring(st.globalIndex) + L")") : L"Local";

    std::wstring out;
    out.reserve(8192);

    out += L"Quest\r\n";
    out += L"  Base:   " + winutil::WidenUtf8(q.baseName) + L"\r\n";
    out += L"  Guild:  " + winutil::WidenUtf8(q.guildName) + L"\r\n";
    out += L"  Name:   " + winutil::WidenUtf8(q.displayName) + L"\r\n\r\n";

    out += L"Stage\r\n";
    out += L"  Stage:      " + std::to_wstring(stageNum) + L"\r\n";
    out += L"  StateIndex: " + std::to_wstring(stateIdx) + L"\r\n";
    out += L"  FlagIndex:  " + (st.flagIndex == 0xFFFFu ? std::wstring(L"(none)") : std::to_wstring(st.flagIndex)) + L"\r\n";
    out += L"  Scope:      " + scope + L"\r\n";
    out += L"  Variable:   " + varName + L"\r\n";
    out += L"  TextVar:    " + HexU32(st.textVarHash) + L"\r\n\r\n";

    std::vector<uint16_t> logIds;
    logIds.reserve(8);

	for (const auto& op : q.qbn.opcodes) {
        if (op.opCode != 0x0017u)
            continue;
        if (op.messageId == 0x0000u || op.messageId == 0xFFFFu)
            continue;
        int gate = stateIdxFromSub(op.sub[0]);
        if (gate == stateIdx)
            logIds.push_back(op.messageId);
    }

    if (!logIds.empty()) {
        std::sort(logIds.begin(), logIds.end());
        logIds.erase(std::unique(logIds.begin(), logIds.end()), logIds.end());

        out += L"Log Entries\r\n";
        for (uint16_t id : logIds) {
            std::wstring msg = qrcPlain(id);
            out += L"  " + HexU16(id);
            if (!msg.empty())
                out += L"  " + msg;
            out += L"\r\n";
        }
        out += L"\r\n";
    }

    out += L"Stage References\r\n";

    int shown = 0;
    const int kMaxShown = 250;

	for (const auto& op : q.qbn.opcodes) {
        int gate = stateIdxFromSub(op.sub[0]);
        bool isGate = (gate == stateIdx);

        bool isTarget = false;
        for (int i = 1; i < 5; i++) {
            int t = stateIdxFromSub(op.sub[i]);
            if (t == stateIdx) {
                isTarget = true;
                break;
            }
        }

        if (!isGate && !isTarget)
            continue;

        if (shown++ >= kMaxShown) {
            out += L"  ... truncated ...\r\n";
            break;
        }

        out += L"\r\n";
        out += isGate ? L"[Gate] " : L"[Ref ] ";
        out += L"off=" + HexU32(op.fileOffset);
        out += L" op=" + HexU16(op.opCode);
		out += L" recs=" + std::to_wstring(op.records);
		out += L" f=" + HexU16(op.flags);
        if (op.messageId != 0x0000u && op.messageId != 0xFFFFu) {
            out += L" msg=" + HexU16(op.messageId);
            std::wstring msg = qrcPlain(op.messageId);
            if (!msg.empty())
                out += L" \"" + msg + L"\"";
        }
        out += L"\r\n";

        out += L"  gate: " + FormatSubRefPretty(q, op.sub[0]) + L"\r\n";
        for (int i = 1; i < 5; i++) {
            if (op.sub[i].sectionId == 0xFFu)
                continue;
            out += L"  s" + std::to_wstring(i) + L":   " + FormatSubRefPretty(q, op.sub[i]) + L"\r\n";
        }
    }

    SetWindowTextW(m_preview, out.c_str());
}



void MainWindow::PopulateQuestText(size_t questIdx)
{
    if (!m_questsLoaded || questIdx >= m_quests.quests.size())
        return;

    std::wstring err;
    if (!m_quests.EnsureQrcLoaded(questIdx, &err))
        return;

    auto& q = m_quests.quests[questIdx];

    SetupListColumns_QuestText();
    ListView_DeleteAllItems(m_list);

    auto trimOneLine = [](std::wstring s, size_t maxChars) {
        for (auto& ch : s) {
            if (ch == L'\r' || ch == L'\n' || ch == L'\t') ch = L' ';
        }
        while (!s.empty() && s.front() == L' ') s.erase(s.begin());
        while (!s.empty() && s.back() == L' ') s.pop_back();
        if (s.size() > maxChars) {
            s.resize(maxChars);
            if (!s.empty()) s.back() = (wchar_t)0x2026; // ellipsis
        }
        return s;
    };

    int row = 0;
    for (auto& rec : q.qrc.records) {
        rec.EnsureParsed(q.qrc.fileBytes);

        std::wstring id = std::to_wstring(rec.recordId);
        std::wstring type = WellKnownQrcType(rec.recordId);

        std::wstring preview;
        if (!rec.subrecords.empty()) {
            auto& sr0 = rec.subrecords[0];
            auto& tt = sr0.EnsureTokens();
            const std::string& base = sr0.hasUserOverride ? sr0.userOverride : tt.plain;
            std::string applied = ApplyOverrides(base);
            preview = trimOneLine(winutil::WidenUtf8(applied), 96);
        }

        LVITEMW lvi{};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = row;
        lvi.pszText = const_cast<LPWSTR>(id.c_str());
        lvi.lParam = static_cast<LPARAM>(rec.recordId);
        const int idx = ListView_InsertItem(m_list, &lvi);
        if (idx >= 0) {
            ListView_SetItemText(m_list, idx, 1, const_cast<LPWSTR>(type.c_str()));
            ListView_SetItemText(m_list, idx, 2, const_cast<LPWSTR>(preview.c_str()));
            row++;
        }
    }

    if (row > 0) {
        ListView_SetItemState(m_list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ShowQuestTextPreview(questIdx, 0);
    } else {
        SetWindowTextW(m_preview, L"");
    }
}

void MainWindow::ShowQuestTextPreview(size_t questIdx, int recRow) {
    if (!m_questsLoaded || questIdx >= m_quests.quests.size()) return;
    auto& q = m_quests.quests[questIdx];
    if (!q.qrcLoaded) return;
    if (recRow < 0 || recRow >= (int)q.qrc.records.size()) return;

    auto& r = q.qrc.records[recRow];
    r.EnsureParsed(q.qrc.fileBytes);

    std::wstring msg;
    msg += L"Quest: " + winutil::WidenUtf8(q.baseName) + L"  (" + winutil::WidenUtf8(q.guildName) + L")\r\n";
    msg += L"Record: " + HexU16(r.recordId) + L"  " + WellKnownQrcType(r.recordId) + L"\r\n\r\n";

    for (size_t i = 0; i < r.subrecords.size(); ++i) {
        auto& sr = r.subrecords[i];
        auto& tt = sr.EnsureTokens();
        const std::string& base = sr.hasUserOverride ? sr.userOverride : tt.plain;
        std::wstring line = winutil::WidenUtf8(ApplyOverrides(base));
        msg += L"[" + std::to_wstring(i) + L"] " + line + L"\r\n";
    }

    SetWindowTextW(m_preview, msg.c_str());
}


void MainWindow::OnQuestTabChanged() {
    if (!m_questsLoaded || m_activeQuest == (size_t)-1) return;
    if (m_activeQuest >= m_quests.quests.size()) return;

    int cur = TabCtrl_GetCurSel(m_tabs);
    if (cur < 0) cur = 1;

    if (cur == 0) {
        // Data
        m_viewMode = ViewMode::QuestData;
        ListView_DeleteAllItems(m_list);
        while (ListView_DeleteColumn(m_list, 0)) {}

        auto& q = m_quests.quests[m_activeQuest];
        std::wstring msg;
        msg += L"Quest: " + winutil::WidenUtf8(q.baseName) + L"\r\n";
        if (!q.displayName.empty()) msg += L"Name: " + winutil::WidenUtf8(q.displayName) + L"\r\n";
        if (q.displayNameSourceRecord) {
            wchar_t b[32]{};
            swprintf_s(b, L"0x%04X", (unsigned)q.displayNameSourceRecord);
            msg += L"Name Source: " + std::wstring(b) + L"\r\n";
        }
        msg += L"\r\n";
        msg += L"QBN: " + q.qbnPath.wstring() + L"\r\n";
        msg += L"QRC: " + q.qrcPath.wstring() + L"\r\n";
        msg += L"\r\n";
        msg += L"States: " + std::to_wstring(q.qbn.states.size()) + L"\r\n";
        msg += L"OpCodes: " + std::to_wstring(q.qbn.opcodes.size()) + L"\r\n";
        msg += L"TextVars: " + std::to_wstring(q.qbn.textVars.size()) + L"\r\n";

        SetWindowTextW(m_preview, msg.c_str());
        return;
    }

    if (cur == 1) {
        PopulateQuestStages(m_activeQuest);
        return;
    }

    if (cur == 2) {
        PopulateQuestText(m_activeQuest);
        return;
    }
}

void MainWindow::ShowQuestView(bool show) {
    if (!m_tabs) return;
    ShowWindow(m_tabs, show ? SW_SHOW : SW_HIDE);
    if (!show) { m_activeQuest = (size_t)-1; }
    // Force relayout
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    OnSize(rc.right - rc.left, rc.bottom - rc.top);
}

void MainWindow::ActivateQuest(size_t questIdx) {
    if (!m_questsLoaded || questIdx >= m_quests.quests.size()) return;
    m_activeQuest = questIdx;

    // Best-effort: load QRC so we can derive a friendly quest name.
    m_quests.EnsureQrcLoaded(questIdx, nullptr);

    // If we can derive a name, update the selected tree item's label in-place.
    ShowQuestView(true);

    int cur = TabCtrl_GetCurSel(m_tabs);
    if (cur < 0) cur = 1; // default to Stages
    TabCtrl_SetCurSel(m_tabs, cur);

    OnQuestTabChanged();
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
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
    case WM_TIMER:
        if (wParam == TIMER_POP_TREE) { self->TreeBuildTick(); return 0; }
        break;
    case WM_APP_LOAD_DONE:
        self->OnLoadDone(reinterpret_cast<LoadResult*>(wParam));
        return 0;
    case WM_COMMAND:
        self->OnCommand(LOWORD(wParam));
        return 0;
    case WM_CONTEXTMENU:
        if ((HWND)wParam == self->m_list) {
            self->OnBsaDialogueListContextMenu(lParam);
            return 0;
        }
        break;
    case WM_NOTIFY: {
        auto* hdr = (LPNMHDR)lParam;

        if (hdr->idFrom == IDC_MAIN_TREE && hdr->code == TVN_SELCHANGEDW) {
            self->OnTreeSelChanged();
            return 0;
        }

        if (hdr->idFrom == IDC_QUEST_TABS && hdr->code == TCN_SELCHANGE) {
            self->OnQuestTabChanged();
            return 0;
        }

        if (hdr->idFrom == IDC_SUBRECORD_LIST && hdr->code == NM_DBLCLK) {
            auto* act = (NMITEMACTIVATE*)lParam;
            if (act) self->BeginListPreviewEdit(act->iItem, act->iSubItem);
            return 0;
        }

        if (hdr->idFrom == IDC_SUBRECORD_LIST && hdr->code == LVN_ITEMCHANGED) {
            auto* nmlv = (NMLISTVIEW*)lParam;
            if ((nmlv->uChanged & LVIF_STATE) && (nmlv->uNewState & LVIS_SELECTED)) {
                if (self->m_viewMode == ViewMode::TextSubrecords) {
                    auto* pld = self->GetSelectedPayload();
                    if (!pld || pld->kind != TreePayload::Kind::TextRecord) return 0;
                    auto* rec = self->m_text.FindMutable(pld->textRecordId);
                    if (!rec) return 0;
                    self->ShowSubrecordPreview(*rec, nmlv->iItem);
                    return 0;
                }

                if (self->m_viewMode == ViewMode::QuestStages) {
                    if (self->m_activeQuest != (size_t)-1) self->ShowQuestStagePreview(self->m_activeQuest, nmlv->iItem);
                    return 0;
                }

                if (self->m_viewMode == ViewMode::QuestText) {
                    if (self->m_activeQuest != (size_t)-1) self->ShowQuestTextPreview(self->m_activeQuest, nmlv->iItem);
                    return 0;
                }

                if (self->m_viewMode == ViewMode::BsaEntry) {
                    self->ShowBsaDialogueRowPreview(nmlv->iItem);
                    return 0;
                }
            }
            return 0;
        }

        break;
    }
    case WM_LBUTTONDOWN:
        self->m_splitLR.OnMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        self->m_splitLR.OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONUP:
        self->m_splitLR.OnMouseUp();
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace ui
