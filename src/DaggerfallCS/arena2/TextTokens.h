#pragma once
#include "../pch.h"

namespace arena2 {

enum class TokenType : uint8_t {
    Text,
    NewLine,
    Position,
    EndOfPage,
    EndOfLineLeft,
    EndOfLineCenter,
    Font,
    Color,
    BookImage,
    Unknown
};

struct Token {
    TokenType type{};
    uint32_t  arg0{};
    uint32_t  arg1{};
    std::string text; // for TokenType::Text or BookImage name
    size_t byteOffset{};
};

enum class VarStyle : uint8_t { Percent, Underscore };

struct VarRef {
    VarStyle style{};
    std::string token;   // e.g. %npc or _npc_
    std::string name;    // e.g. npc
    uint32_t hash{};     // shift-add hash
    size_t plainOffset{}; // offset in TokenizedText.plain
    size_t byteOffset{};  // offset in raw subrecord bytes
};

struct TokenizedText {
    std::vector<Token> tokens;
    std::vector<VarRef> vars;
    std::string plain;
    std::string rich;
    bool hasEndOfPage{};
    bool hasFontScript{};
};

TokenizedText TokenizeTextSubrecord(const std::vector<uint8_t>& bytes);

}
