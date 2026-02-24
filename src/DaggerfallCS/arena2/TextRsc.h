#pragma once
#include "../pch.h"
#include "TextTokens.h"

namespace arena2 {

struct TextSubrecord {
    std::vector<uint8_t> raw;

    // Non-persistent UI override (UTF-8). Used for TES4-compliant viewing/export without mutating source files.
    std::string userOverride;
    bool hasUserOverride{ false };

    TokenizedText tok{};
    bool tokReady{ false };

    inline TokenizedText& EnsureTokens() {
        if (!tokReady) {
            tok = TokenizeTextSubrecord(raw);
            tokReady = true;
        }
        return tok;
    }

    inline void ClearOverride() {
        userOverride.clear();
        hasUserOverride = false;
    }
    inline void SetOverride(std::string v) {
        userOverride = std::move(v);
        hasUserOverride = true;
    }

    inline const std::string& EffectivePlain() {
        auto& t = EnsureTokens();
        return hasUserOverride ? userOverride : t.plain;
    }
    inline const std::string& EffectiveRich() {
        auto& t = EnsureTokens();
        return hasUserOverride ? userOverride : t.rich;
    }
};


struct TextRecord {
    uint16_t recordId{};
    uint32_t start{};
    uint32_t end{};

    std::vector<TextSubrecord> subrecords;
    bool parsed{ false };

    void EnsureParsed(const std::vector<uint8_t>& fileBytes);
};

struct TextRsc {
    std::filesystem::path sourcePath;

    // Keep bytes resident so records can be parsed lazily.
    std::vector<uint8_t> fileBytes;

    // Records are indexed at load-time; subrecords are parsed on demand.
    std::vector<TextRecord> records;

        static bool LoadFromFile(const std::filesystem::path& filePath, TextRsc& out, std::wstring* err);

static bool LoadFromArena2Root(const std::filesystem::path& arena2Root, TextRsc& out, std::wstring* err);

    const TextRecord* Find(uint16_t id) const;
    TextRecord* FindMutable(uint16_t id);
};

}
