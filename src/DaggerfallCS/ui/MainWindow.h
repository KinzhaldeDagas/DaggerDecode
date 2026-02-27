#pragma once
#include "../pch.h"
#include "../arena2/TextRsc.h"
#include "../arena2/TextRscIndex.h"
#include "../arena2/VarHashCatalog.h"
#include "../arena2/QuestCatalog.h"
#include "../battlespire/BattlespireFormats.h"
#include "Splitter.h"
#include "IndicesPrefsWindow.h"

namespace ui {

constexpr UINT WM_APP_LOAD_DONE = WM_APP + 1;
constexpr UINT_PTR TIMER_POP_TREE = 1;

class MainWindow {
public:
    struct IndicesRow {
        std::string token;        // exact token, e.g. "%ra" or "_fx1_"
        std::string handler;
        std::string macroType;    // "singleline" | "multiline"
        bool implemented{ false };
        std::string comment;

        std::wstring tokenW;
        std::wstring descW;

        std::wstring handlerW;
        std::wstring typeW;
        std::wstring implW;
        std::wstring commentW;

        std::wstring valueW;
    };

    const std::vector<IndicesRow>& GetIndicesRows() const { return m_indicesRows; }
    void SetIndexOverrideByRow(int row, const std::wstring& value);

    bool Create(HINSTANCE hInst);
    HWND Hwnd() const { return m_hwnd; }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    enum class ViewMode { TextSubrecords, QuestData, QuestStages, QuestText, BsaEntry, None };
    enum class BsaDialogueKind { None, Script, Routing, Table };

    struct FlcSpeakTarget {
        std::wstring code;
        std::wstring lineText;
        bool replySide{ false };
    };

    struct TreePayload {
        enum class Kind { TextRecord, QuestsRoot, Quest, QuestStages, QuestText, BsaRoot, BsaArchive, BsaEntry };
        Kind kind{ Kind::TextRecord };
        uint16_t textRecordId{};
        size_t questIndex{};
        size_t bsaArchiveIndex{};
        size_t bsaEntryIndex{};
    };

    static constexpr LPARAM kRecordTag = 0x10000;

    struct LoadResult;
    void OnLoadDone(LoadResult* r);

    HWND m_hwnd{};
    HWND m_tree{};
    HWND m_list{};
    HWND m_preview{};
    HWND m_tabs{};
    HWND m_status{};

    // In-place editing for list Preview cells (non-persistent per-subrecord override layer)
    HWND m_inplaceEdit{};
    WNDPROC m_inplaceEditOldProc{};
    int m_inplaceItem{ -1 };
    int m_inplaceSubItem{ -1 };
    bool m_inplaceEnding{ false };


    Splitter m_splitLR{};
    Splitter m_splitRight{};

    // TEXT.RSC
    arena2::TextRsc m_text;
    arena2::IndexCatalog m_index;
    bool m_indexLoaded{ false };

    // Variable hashes (TEXT.RSC + QBN)
    arena2::VarHashCatalog m_varHash;
    bool m_varHashLoaded{ false };

    // Quests
    arena2::QuestCatalog m_quests;
    bool m_questsLoaded{ false };

    // Battlespire BSA archives
    std::vector<battlespire::BsaArchive> m_bsaArchives;
    bool m_bsaLoaded{ false };

    std::atomic_bool m_loading{ false };

    // Tree model (payload-backed)
    HTREEITEM m_treeRootText{};
    HTREEITEM m_treeRootQuests{};
    HTREEITEM m_treeRootBsa{};
    std::vector<std::unique_ptr<TreePayload>> m_treePayloads;

    // Incremental insertion queues
    std::vector<uint16_t> m_pendingTreeIds;
    std::vector<HTREEITEM> m_pendingTreeParents;
    size_t m_treeInsertPos{ 0 };

    // Book support
    std::unordered_set<uint16_t> m_bookRecordIds;
    std::unordered_map<uint16_t, std::wstring> m_bookRecordTitles;

    std::vector<size_t> m_pendingQuestIdx;
    std::vector<HTREEITEM> m_pendingQuestParents;
    size_t m_questInsertPos{ 0 };

    // BSA dialogue viewer state
    BsaDialogueKind m_bsaDialogueKind{ BsaDialogueKind::None };
    std::vector<std::wstring> m_bsaDialogueHeaders;
    std::vector<std::vector<std::wstring>> m_bsaDialogueRows;
    std::wstring m_bsaDialogueTitle;

    ViewMode m_viewMode{ ViewMode::None };
    size_t m_activeQuest{ (size_t)-1 };

    // Indices (macro) substitution layer
    std::unordered_map<std::string, std::string> m_varOverrides;   // token -> value (exact token match)
    std::unordered_map<std::string, bool> m_varImplemented;        // token -> implemented
    std::vector<IndicesRow> m_indicesRows;
    IndicesPrefsWindow m_indicesWnd;

    void InitIndicesModel();
    void LoadIndicesOverrides();
    void SaveIndicesOverrides();
    std::string ApplyOverrides(std::string_view in) const;
    void RefreshPreviewIfVisible();
    void NoteDiscoveredVars(const arena2::TokenizedText& tt);

    bool OnCreate();
    void OnDestroy();
    void OnSize(int cx, int cy);
    void OnCommand(WORD id);

    void OnTreeSelChanged();
    void OnQuestTabChanged();
    void ShowQuestView(bool show);
    void ActivateQuest(size_t questIdx);
    void PopulateTree();
    void StartTreeBuild();
    void TreeBuildTick();
    void PopulateSubrecordList(arena2::TextRecord& rec);
    void ShowSubrecordPreview(arena2::TextRecord& rec, int index);

    // In-place editing (Preview column double-click)
    void BeginListPreviewEdit(int item, int subItem);
    void EndListPreviewEdit(bool commit);
    void CommitListPreviewEdit(int item, int subItem, const std::wstring& newText);
    static LRESULT CALLBACK InplaceEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);


    // Quests (UI)
    TreePayload* AddPayload(TreePayload::Kind kind, uint16_t recId = 0, size_t questIdx = (size_t)-1, size_t bsaArchiveIdx = (size_t)-1, size_t bsaEntryIdx = (size_t)-1);
    TreePayload* GetSelectedPayload() const;

    void SetupListColumns_TextSubrecords();
    void SetupListColumns_BsaDialogueScript();
    void SetupListColumns_BsaDialogueRouting();
    void SetupListColumns_BsaTable(const std::vector<std::wstring>& headers);
    void SetupListColumns_QuestStages();
    void SetupListColumns_QuestText();

    void ShowBsaDialogueRowPreview(int row);
    void OnBsaDialogueListContextMenu(LPARAM lParam);
    void OnTreeContextMenu(LPARAM lParam);
    bool BuildFlcSpeakTargetFromListClick(int row, int subItem, FlcSpeakTarget& out) const;
    void CmdSpeakBsaDialogueLine(const FlcSpeakTarget& target);
    void CmdSpeakSelectedFlcEntry();

    void PopulateQuestStages(size_t questIdx);
    void PopulateQuestText(size_t questIdx);
    void ShowQuestStagePreview(size_t questIdx, int stageRow);
    void ShowQuestTextPreview(size_t questIdx, int recRow);

    // Commands
    void CmdOpenArena2();
    void CmdOpenSpire();
    void CmdExtractBsa();
    void CmdExportSubrecords();
    void CmdExportTokens();
    void CmdExportVariables();
    void CmdExportQuests();
    void CmdExportQuestStages();
    void CmdExportTes4QuestDialogue();

    void SetStatus(const std::wstring& s);
};

} // namespace ui
