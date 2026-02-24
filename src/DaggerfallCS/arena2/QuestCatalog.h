#pragma once
#include "../pch.h"
#include "QuestQbn.h"
#include "TextRsc.h"

namespace arena2 {

struct QuestEntry {
    std::string baseName; // e.g. "A0B00Y00"
    std::filesystem::path qbnPath;
    std::filesystem::path qrcPath;

    // Filename-derived metadata (best-effort).
    char guildCode{};
    char membershipCode{};
    char minRepCode{};
    char childGuardCode{};
    char deliveryCode{};

    // Decoded/friendly metadata
    std::string guildName;
    std::string membershipName;
    std::string deliveryName;
    int minRepValue{ -1 }; // digit * 10 (best-effort)
    bool childGuardRestricted{ false };
    bool standardFilename{ false }; 

    // Quest display name derived from QRC/QBN (best-effort)
    std::string displayName;
    uint16_t displayNameSourceRecord{ 0 }; // QRC record used, 0 if unknown


    QuestQbn qbn;
    TextRsc qrc;
	bool qbnLoaded{ false };
    bool qrcLoaded{ false };
};

struct QuestCatalog {
    static const char* GuildNameForCode(char c);
    static const char* MembershipNameForCode(char c);
    static const char* DeliveryNameForCode(char c);
    static bool IsStandardQuestFilename(const std::string& baseName);
    std::filesystem::path arena2Root;
    std::vector<QuestEntry> quests;
	const VarHashCatalog* hashes{ nullptr };

    bool LoadFromArena2Root(const std::filesystem::path& folder, const VarHashCatalog* hashes, std::wstring* err);
    // Compatibility overloads: callers that don't have a hash catalog handy.
    bool LoadFromArena2Root(const std::filesystem::path& folder, std::wstring* err) {
        return LoadFromArena2Root(folder, nullptr, err);
    }
    bool LoadFromArena2Root(const std::filesystem::path& folder) {
        return LoadFromArena2Root(folder, nullptr, nullptr);
    }
    bool EnsureQrcLoaded(size_t questIndex, std::wstring* err);
};

} // namespace arena2
