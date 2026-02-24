#include "pch.h"
#include "TextTokens.h"
#include "VarHashCatalog.h"

namespace arena2 {

static void AppendPlain(TokenizedText& t, std::string_view s) { t.plain.append(s); }
static void AppendRich(TokenizedText& t, std::string_view s) { t.rich.append(s); }

static bool IsVarChar(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static std::string ToLowerAscii(std::string_view s) {
    std::string out(s);
    for (auto& c : out) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return out;
}

static void DetectVarsInText(TokenizedText& t, std::string_view text, size_t plainBase, size_t byteBase) {
    // %name
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == '%') {
            size_t j = i + 1;
            if (j >= text.size()) continue;
            if (!IsVarChar((unsigned char)text[j])) continue;

            while (j < text.size() && IsVarChar((unsigned char)text[j])) j++;

            std::string name = ToLowerAscii(text.substr(i + 1, j - (i + 1)));
            if (name.empty()) continue;

            VarRef vr{};
            vr.style = VarStyle::Percent;
            vr.token = std::string(text.substr(i, j - i));
            vr.name = name;
            vr.hash = ComputeVarHash(name);
            vr.plainOffset = plainBase + i;
            vr.byteOffset = byteBase + i;
            t.vars.push_back(std::move(vr));
            i = j - 1;
            continue;
        }

        // _name_ (leading underscore, trailing underscore)
        if (c == '_') {
            size_t j = i + 1;
            if (j >= text.size()) continue;
            if (!IsVarChar((unsigned char)text[j])) continue;

            while (j < text.size() && IsVarChar((unsigned char)text[j])) j++;
            if (j >= text.size()) continue;
            if (text[j] != '_') continue;

            std::string name = ToLowerAscii(text.substr(i + 1, j - (i + 1)));
            if (name.empty()) continue;

            VarRef vr{};
            vr.style = VarStyle::Underscore;
            vr.token = std::string(text.substr(i, (j + 1) - i));
            vr.name = name;
            vr.hash = ComputeVarHash(name);
            vr.plainOffset = plainBase + i;
            vr.byteOffset = byteBase + i;
            t.vars.push_back(std::move(vr));

            i = j; // will increment to j+1
            continue;
        }
    }
}

static void PushText(TokenizedText& t, std::string_view s, size_t byteOffsetStart) {
    if (s.empty()) return;

    size_t plainBase = t.plain.size();

    Token tok{};
    tok.type = TokenType::Text;
    tok.text = std::string(s);
    tok.byteOffset = byteOffsetStart;
    t.tokens.push_back(tok);

    AppendPlain(t, s);
    AppendRich(t, s);

    DetectVarsInText(t, s, plainBase, byteOffsetStart);
}

static void PushSimple(TokenizedText& t, TokenType type, uint32_t a0, uint32_t a1, size_t off) {
    Token tok{};
    tok.type = type;
    tok.arg0 = a0;
    tok.arg1 = a1;
    tok.byteOffset = off;
    t.tokens.push_back(std::move(tok));
}

static std::string HexByte(uint8_t b) {
    static const char* hexd = "0123456789ABCDEF";
    std::string s;
    s.push_back(hexd[(b >> 4) & 0xF]);
    s.push_back(hexd[b & 0xF]);
    return s;
}

TokenizedText TokenizeTextSubrecord(const std::vector<uint8_t>& bytes) {
    TokenizedText out{};
    std::string run;
    run.reserve(bytes.size());

    size_t runStart = 0;
    bool runActive = false;

    auto flushRun = [&](size_t curOff) {
        (void)curOff;
        if (!run.empty() && runActive) {
            PushText(out, run, runStart);
        }
        run.clear();
        runActive = false;
    };

    for (size_t i = 0; i < bytes.size(); ++i) {
        uint8_t b = bytes[i];

        // PositionCode: (NewLineOffset/SameLineOffset/PullPreceeding) 0xFB X Y
        // Note: "greedy" parsing means values like 0xFC 0xFB ... are PositionCode, not EndOfLineLeft.
        if ((i + 3) < bytes.size() && bytes[i + 1] == 0xFB) {
            uint8_t mode = bytes[i];       // 0x00 new line offset, 0x01 same line offset, 0x02-0xFF pull preceding
            uint8_t x = bytes[i + 2];
            uint8_t y = bytes[i + 3];
            flushRun(i);
            PushSimple(out, TokenType::Position, mode, (uint32_t)(x | (uint32_t(y) << 8)), i);

            // Best-effort plain reconstruction: PositionCode with NewLineOffset implies a newline.
            if (mode == 0x00) out.plain.push_back('\n');

            out.rich.append("<pos m=0x");
            out.rich.append(HexByte(mode));
            out.rich.append(" x=");
            out.rich.append(std::to_string((unsigned)x));
            out.rich.append(" y=");
            out.rich.append(std::to_string((unsigned)y));
            out.rich.append(">");

            i += 3;
            continue;
        }

        // EndOfLine codes: 0xFC 0x00 or 0xFD 0x00
        if ((b == 0xFC || b == 0xFD) && (i + 1 < bytes.size()) && bytes[i + 1] == 0x00) {
            flushRun(i);
            if (b == 0xFC) PushSimple(out, TokenType::EndOfLineLeft, 0, 0, i);
            else PushSimple(out, TokenType::EndOfLineCenter, 0, 0, i);

            out.plain.push_back('\n');
            out.rich.append("\n");
            i += 1;
            continue;
        }

        // NewLine token
        if (b == 0x00) {
            flushRun(i);
            PushSimple(out, TokenType::NewLine, 0, 0, i);
            out.plain.push_back('\n');
            out.rich.append("\n");
            continue;
        }

        // EndOfPage
        if (b == 0xF6) {
            flushRun(i);
            PushSimple(out, TokenType::EndOfPage, 0, 0, i);
            out.hasEndOfPage = true;
            out.plain.append("\n\f\n");
            out.rich.append("\n<page/>\n");
            continue;
        }

        // FontPrefix (0xF9) + font byte
        if (b == 0xF9 && (i + 1 < bytes.size())) {
            uint8_t font = bytes[i + 1];
            flushRun(i);
            PushSimple(out, TokenType::Font, font, 0, i);
            if (font == 0x02) out.hasFontScript = true;

            if (font == 0x02) out.rich.append("<font=script>");
            else if (font == 0x04) out.rich.append("<font=normal>");
            else {
                out.rich.append("<font=0x");
                out.rich.append(HexByte(font));
                out.rich.append(">");
            }
            i += 1;
            continue;
        }

        // FontColor (0xFA) + color index
        if (b == 0xFA && (i + 1 < bytes.size())) {
            uint8_t color = bytes[i + 1];
            flushRun(i);
            PushSimple(out, TokenType::Color, color, 0, i);
            out.rich.append("<color=");
            out.rich.append(std::to_string((unsigned)color));
            out.rich.append(">");
            i += 1;
            continue;
        }

        // BookImage: 0xF7 + zero-terminated IMG name
        if (b == 0xF7) {
            flushRun(i);
            std::string name;
            size_t j = i + 1;
            while (j < bytes.size() && bytes[j] != 0x00 && name.size() < 255) {
                name.push_back((char)bytes[j]);
                j++;
            }
            Token tok{};
            tok.type = TokenType::BookImage;
            tok.text = name;
            tok.byteOffset = i;
            out.tokens.push_back(std::move(tok));
            out.rich.append("<bookimg=");
            out.rich.append(name);
            out.rich.append(">");
            i = (j < bytes.size()) ? j : (bytes.size() - 1);
            continue;
        }

        // Printable ASCII: accumulate
        if (b >= 0x20 && b <= 0x7F) {
            if (!runActive) {
                runActive = true;
                runStart = i;
            }
            run.push_back((char)b);
        } else {
            flushRun(i);
            PushSimple(out, TokenType::Unknown, b, 0, i);
            out.rich.append("<0x");
            out.rich.append(HexByte(b));
            out.rich.append(">");
        }
    }

    flushRun(bytes.size());
    return out;
}

} // namespace arena2
