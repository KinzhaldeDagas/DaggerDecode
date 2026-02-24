#pragma once
#include "../pch.h"
#include "VarHashCatalog.h"

namespace arena2 {

struct QbnHeader {
    uint16_t questId{};
    uint16_t factionId{};
    uint16_t resourceId{};
    std::array<uint8_t, 9> resourceFilename{};
    uint8_t hasDebugInfo{};

    std::array<uint16_t, 10> sectionRecordCount{};
    std::array<uint16_t, 11> sectionOffset{};
};


struct QbnSubRecord {
    uint8_t notFlag{};
    uint32_t localPtr{};
    uint16_t sectionId{};
    uint32_t value{};
    uint32_t objectPtr{};

    // Best-effort record index for section references.
    // Prefer Value when it looks valid; otherwise fall back to low byte of LocalPtr.
    int RecordIndex() const {
        if (value != 0xFFFFFFFFu && value != 0xFFFFFFFEu) return (int)value;
        uint8_t lo = (uint8_t)(localPtr & 0xFFu);
        if (lo == 0xFFu) return -1;
        return (int)lo;
    }
};

struct QbnOpCodeRecord {
    uint16_t opCode{};
    uint16_t flags{};
    uint16_t records{}; // number of valid sub-records (1..5)
    std::array<QbnSubRecord, 5> sub{};
    uint16_t messageId{}; // 0xFFFF = none
    uint32_t lastUpdate{};
    uint32_t fileOffset{}; // byte offset in QBN for debugging
};

struct QbnState {
    int16_t flagIndex{};
    uint8_t isGlobal{};
    uint8_t globalIndex{};
    uint32_t textVarHash{};

    // Resolved names (lowercase, no underscores) from catalogs.
    std::vector<std::string> varNames;
};

struct QbnTextVariable {
    std::string nameLower;   // as stored (lowercase)
    uint8_t sectionId{};
    uint16_t recordId{};
    uint32_t recordPtr{};
    uint32_t hash{};

    // Resolved names (lowercase, no underscores) from catalogs.
    std::vector<std::string> varNames;
};

struct QbnItem {
    int16_t itemIndex{};
    uint8_t reward{};
    uint16_t itemCategory{};
    uint16_t itemCategoryIndex{};
    uint32_t textVarHash{};
    uint16_t textRecordId1{};
    uint16_t textRecordId2{};

    std::vector<std::string> varNames;
};

struct QbnNpc {
    int16_t npcIndex{};
    uint8_t gender{};
    uint8_t faceIndex{};
    uint16_t unknown1{};
    uint16_t factionIndex{};
    uint32_t textVarHash{};
    uint16_t textRecordId1{};
    uint16_t textRecordId2{};

    std::vector<std::string> varNames;
};

struct QbnLocation {
    uint16_t locationIndex{};
    uint8_t flags{};
    uint8_t generalLocation{};
    uint16_t fineLocation{};
    int16_t locationType{};
    int16_t doorSelector{};
    uint16_t unknown2{};
    uint32_t textVarHash{};
    uint32_t objPtr{};
    uint16_t textRecordId1{};
    uint16_t textRecordId2{};

    std::vector<std::string> varNames;
};

struct QbnTimer {
    int16_t timerIndex{};
    uint16_t flags{};
    uint8_t type{};
    int32_t minimum{};
    int32_t maximum{};
    uint32_t started{};
    uint32_t duration{};
    int32_t link1{};
    int32_t link2{};
    uint32_t textVarHash{};

    std::vector<std::string> varNames;
};

struct QbnMob {
    uint8_t mobIndex{};
    uint16_t null1{};
    uint8_t mobType{};
    uint16_t mobCount{};
    uint32_t textVarHash{};
    uint32_t null2{};

    std::vector<std::string> varNames;
};

struct QuestQbn {

    std::filesystem::path sourcePath;
    std::vector<uint8_t> fileBytes;

    QbnHeader header{};
    std::vector<QbnState> states;
    std::vector<QbnTextVariable> textVars;
    std::vector<QbnOpCodeRecord> opcodes;


    std::vector<QbnItem> items;
    std::vector<QbnNpc> npcs;
    std::vector<QbnLocation> locations;
    std::vector<QbnTimer> timers;
    std::vector<QbnMob> mobs;

    // Index maps: index -> vector position
    std::unordered_map<uint16_t, size_t> itemByIndex;
    std::unordered_map<uint16_t, size_t> npcByIndex;
    std::unordered_map<uint16_t, size_t> locationByIndex;
    std::unordered_map<uint16_t, size_t> timerByIndex;
    std::unordered_map<uint16_t, size_t> mobByIndex;

    const QbnItem* FindItem(uint16_t idx) const {
        auto it = itemByIndex.find(idx);
        return (it == itemByIndex.end()) ? nullptr : &items[it->second];
    }
    const QbnNpc* FindNpc(uint16_t idx) const {
        auto it = npcByIndex.find(idx);
        return (it == npcByIndex.end()) ? nullptr : &npcs[it->second];
    }
    const QbnLocation* FindLocation(uint16_t idx) const {
        auto it = locationByIndex.find(idx);
        return (it == locationByIndex.end()) ? nullptr : &locations[it->second];
    }
    const QbnTimer* FindTimer(uint16_t idx) const {
        auto it = timerByIndex.find(idx);
        return (it == timerByIndex.end()) ? nullptr : &timers[it->second];
    }
    const QbnMob* FindMob(uint16_t idx) const {
        auto it = mobByIndex.find(idx);
        return (it == mobByIndex.end()) ? nullptr : &mobs[it->second];
    }

    void BuildIndexMaps() {
        itemByIndex.clear(); npcByIndex.clear(); locationByIndex.clear(); timerByIndex.clear(); mobByIndex.clear();
        for (size_t i = 0; i < items.size(); ++i) itemByIndex[(uint16_t)items[i].itemIndex] = i;
        for (size_t i = 0; i < npcs.size(); ++i) npcByIndex[(uint16_t)npcs[i].npcIndex] = i;
        for (size_t i = 0; i < locations.size(); ++i) locationByIndex[locations[i].locationIndex] = i;
        for (size_t i = 0; i < timers.size(); ++i) timerByIndex[(uint16_t)timers[i].timerIndex] = i;
        for (size_t i = 0; i < mobs.size(); ++i) mobByIndex[(uint16_t)mobs[i].mobIndex] = i;
    }
    // Primary loader (source of truth). Optional catalogs and error output.
    bool LoadFromFile(const std::filesystem::path& path, const VarHashCatalog* hashes, std::wstring* err);

    // Compatibility overloads: keep UI/tools resilient against signature drift.
    bool LoadFromFile(const std::filesystem::path& path) {
        return LoadFromFile(path, nullptr, nullptr);
    }
    bool LoadFromFile(const std::filesystem::path& path, std::wstring* err) {
        return LoadFromFile(path, nullptr, err);
    }
};

} // namespace arena2
