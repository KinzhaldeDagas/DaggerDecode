#include "pch.h"
#include "TextRsc.h"

namespace arena2 {

static bool ReadAllBytes(const std::filesystem::path& path, std::vector<uint8_t>& out, std::wstring* err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { if (err) *err = L"Failed to open TEXT.RSC."; return false; }
    f.seekg(0, std::ios::end);
    auto sz = f.tellg();
    if (sz <= 0) { if (err) *err = L"TEXT.RSC is empty."; return false; }
    f.seekg(0, std::ios::beg);
    out.resize((size_t)sz);
    f.read((char*)out.data(), sz);
    if (!f.good()) { if (err) *err = L"Failed to read TEXT.RSC."; return false; }
    return true;
}

static uint16_t ReadU16(const std::vector<uint8_t>& b, size_t off) {
    return (uint16_t)(b[off] | (uint16_t(b[off + 1]) << 8));
}
static uint32_t ReadU32(const std::vector<uint8_t>& b, size_t off) {
    return (uint32_t)b[off] | ((uint32_t)b[off + 1] << 8) | ((uint32_t)b[off + 2] << 16) | ((uint32_t)b[off + 3] << 24);
}

void TextRecord::EnsureParsed(const std::vector<uint8_t>& fileBytes) {
    if (parsed) return;
    parsed = true;
    subrecords.clear();

    if (start >= fileBytes.size() || end > fileBytes.size() || end <= start) return;

    std::vector<uint8_t> cur;
    uint32_t p = start;

    while (p < end) {
        uint8_t b = fileBytes[p++];
        if (b == 0xFF) {
            TextSubrecord sr{};
            sr.raw = std::move(cur);
            sr.tokReady = false;
            subrecords.push_back(std::move(sr));
            cur = {};
            continue;
        }
        if (b == 0xFE) {
            TextSubrecord sr{};
            sr.raw = std::move(cur);
            sr.tokReady = false;
            subrecords.push_back(std::move(sr));
            return;
        }
        cur.push_back(b);
    }

    if (!cur.empty()) {
        TextSubrecord sr{};
        sr.raw = std::move(cur);
        sr.tokReady = false;
        subrecords.push_back(std::move(sr));
    }
}

struct HeaderEntry {
    uint16_t id{};
    uint32_t off{};
};

static bool ReadHeaderEntries_Spec(const std::vector<uint8_t>& file, uint16_t headerLen, std::vector<HeaderEntry>& out, uint32_t& outTermOff, std::wstring* err) {
    out.clear();
    outTermOff = (uint32_t)file.size();

    if (file.size() < 8) { if (err) *err = L"TEXT.RSC too small."; return false; }

    // UESP: headerLen is UInt16 length of header info; TextRecordHeader is 0x06 bytes; recordCount = (headerLen/6)-1. 【162:5†...】
    // In the wild, the terminator often behaves like a full 0x06 header (id=0xFFFF + u32 offset),
    // but we accept either 2-byte or 6-byte terminators.
    if (headerLen < 6) { if (err) *err = L"Invalid headerLen (<6)."; return false; }

    size_t n = (size_t)(headerLen / 6); // number of 6-byte slots in header region
    if (n < 2) { if (err) *err = L"Invalid headerLen (too small for at least 1 record + terminator)."; return false; }

    size_t recordCount = n - 1; // per UESP formula
    size_t base = 2;            // immediately after headerLen field

    // First attempt: fixed layout where terminator is the (recordCount)th slot (6 bytes)
    size_t termPos = base + recordCount * 6;
    if (termPos + 2 <= file.size() && ReadU16(file, termPos) == 0xFFFF) {
        // If 6 bytes available, treat as full header and read terminator offset.
        if (termPos + 6 <= file.size()) {
            outTermOff = ReadU32(file, termPos + 2);
        } else {
            outTermOff = (uint32_t)file.size();
        }

        out.reserve(recordCount);
        for (size_t i = 0; i < recordCount; ++i) {
            size_t p = base + i * 6;
            if (p + 6 > file.size()) break;
            uint16_t id = ReadU16(file, p);
            uint32_t off = ReadU32(file, p + 2);
            if (id == 0xFFFF) break;
            out.push_back({ id, off });
        }
    } else {
        // Fallback: scan forward in 6-byte steps to find the 0xFFFF terminator slot.
        // This is aligned with the concept of TextRecordHeaderList terminated by 0xFFFF. 【161:0†...†L4-L13】
        size_t p = base;
        size_t guard = std::min<size_t>(file.size(), base + (size_t)headerLen + 64);
        while (p + 2 <= guard && p + 2 <= file.size()) {
            uint16_t id = ReadU16(file, p);
            if (id == 0xFFFF) {
                if (p + 6 <= file.size()) outTermOff = ReadU32(file, p + 2);
                else outTermOff = (uint32_t)file.size();
                break;
            }
            if (p + 6 > file.size()) break;
            uint32_t off = ReadU32(file, p + 2);
            out.push_back({ id, off });
            p += 6;
            if (out.size() > 50000) break;
        }
    }

    if (out.empty()) { if (err) *err = L"Header parse produced 0 records."; return false; }

    // Validate offsets are in-bounds and non-decreasing
    uint32_t prev = 0;
    for (size_t i = 0; i < out.size(); ++i) {
        uint32_t off = out[i].off;
        if (off >= file.size()) { if (err) *err = L"Header has out-of-range record offset."; return false; }
        if (i && off < prev) { if (err) *err = L"Header offsets not sorted."; return false; }
        prev = off;
    }

    // If terminator offset looks bogus, clamp to file size.
    if (outTermOff == 0 || outTermOff > file.size()) outTermOff = (uint32_t)file.size();
    if (outTermOff < out.back().off) outTermOff = (uint32_t)file.size();

    return true;
}


static bool LoadTextDbFromPath(const std::filesystem::path& path, TextRsc& out, std::wstring* err) {
    out = {};
    out.sourcePath = path;

    if (!ReadAllBytes(path, out.fileBytes, err)) return false;
    const auto& file = out.fileBytes;

    if (file.size() < 8) { if (err) *err = L"Text database too small."; return false; }

    uint16_t headerLen = ReadU16(file, 0);

    std::vector<HeaderEntry> headers;
    uint32_t termOff = (uint32_t)file.size();

    std::wstring perr;
    if (!ReadHeaderEntries_Spec(file, headerLen, headers, termOff, &perr)) {
        if (err) {
            wchar_t buf[512]{};
            swprintf_s(buf, L"Failed to parse text DB header list (headerLen=%u, fileSize=%zu): %s", headerLen, file.size(), perr.c_str());
            *err = buf;
        }
        return false;
    }

    out.records.clear();
    out.records.reserve(headers.size());
    for (size_t i = 0; i < headers.size(); ++i) {
        TextRecord r{};
        r.recordId = headers[i].id;
        r.start = headers[i].off;
        r.end = (i + 1 < headers.size()) ? headers[i + 1].off : termOff;
        if (r.end > (uint32_t)file.size()) r.end = (uint32_t)file.size();
        out.records.push_back(std::move(r));
    }

    return true;
}

bool TextRsc::LoadFromFile(const std::filesystem::path& filePath, TextRsc& out, std::wstring* err) {
    if (!std::filesystem::exists(filePath)) {
        if (err) *err = L"File not found.";
        return false;
    }
    return LoadTextDbFromPath(filePath, out, err);
}

bool TextRsc::LoadFromArena2Root(const std::filesystem::path& arena2Root, TextRsc& out, std::wstring* err) {
    out = {};
    std::filesystem::path root = arena2Root;

    std::filesystem::path candidate = root / "TEXT.RSC";
    if (!std::filesystem::exists(candidate)) {
        std::filesystem::path arena2 = root / "ARENA2";
        candidate = arena2 / "TEXT.RSC";
        if (!std::filesystem::exists(candidate)) {
            if (err) *err = L"Could not find TEXT.RSC (expected in selected folder or in an ARENA2 subfolder).";
            return false;
        }
        root = arena2;
    }

    out.sourcePath = candidate;

    // Parse as generic Text Record Database
    return LoadTextDbFromPath(candidate, out, err);

    }



const TextRecord* TextRsc::Find(uint16_t id) const {
    for (const auto& r : records) if (r.recordId == id) return &r;
    return nullptr;
}

TextRecord* TextRsc::FindMutable(uint16_t id) {
    for (auto& r : records) if (r.recordId == id) return &r;
    return nullptr;
}

} // namespace arena2
