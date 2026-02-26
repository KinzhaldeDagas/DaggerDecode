#include "pch.h"
#include "BattlespireFormats.h"

namespace battlespire {

static bool ReadAllBytes(const std::filesystem::path& path, std::vector<uint8_t>& out, std::wstring* err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (err) *err = L"Failed to open file.";
        return false;
    }

    f.seekg(0, std::ios::end);
    std::streamoff sz = f.tellg();
    if (sz < 0) {
        if (err) *err = L"Failed to read file size.";
        return false;
    }

    f.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(sz));
    if (!out.empty()) {
        f.read(reinterpret_cast<char*>(out.data()), sz);
        if (!f.good()) {
            if (err) *err = L"Failed to read file bytes.";
            return false;
        }
    }

    return true;
}

static uint16_t ReadU16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (uint16_t(p[1]) << 8));
}

static uint32_t ReadU32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

static void WriteLe16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

static void WriteLe32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

bool RawScreenshot::LoadFromFile(const std::filesystem::path& filePath, RawScreenshot& out, std::wstring* err) {
    out = {};
    out.sourcePath = filePath;

    if (!std::filesystem::exists(filePath)) {
        if (err) *err = L"File not found.";
        return false;
    }

    if (!ReadAllBytes(filePath, out.rgb24, err)) {
        return false;
    }

    if (out.rgb24.size() != kExpectedBytes) {
        if (err) {
            wchar_t buf[256]{};
            swprintf_s(buf, L"RAW screenshot size mismatch. Expected %zu bytes but got %zu.", kExpectedBytes, out.rgb24.size());
            *err = buf;
        }
        out = {};
        return false;
    }

    return true;
}

bool WavesPcm::BuildWavFile(const std::vector<uint8_t>& pcm, std::vector<uint8_t>& wavOut, std::wstring* err) {
    wavOut.clear();

    const uint32_t bytesPerSample = uint32_t(kBitsPerSample / 8u);
    const uint32_t byteRate = kSampleRate * uint32_t(kChannels) * bytesPerSample;
    const uint16_t blockAlign = static_cast<uint16_t>(uint16_t(kChannels) * uint16_t(bytesPerSample));

    if (pcm.size() > 0xFFFFFFFFu - 44u) {
        if (err) *err = L"PCM payload too large to package as RIFF/WAVE.";
        return false;
    }

    const uint32_t dataSize = static_cast<uint32_t>(pcm.size());
    const uint32_t riffSize = 36u + dataSize;

    wavOut.reserve(size_t(44u) + pcm.size());

    // RIFF header
    wavOut.insert(wavOut.end(), { 'R','I','F','F' });
    WriteLe32(wavOut, riffSize);
    wavOut.insert(wavOut.end(), { 'W','A','V','E' });

    // fmt chunk
    wavOut.insert(wavOut.end(), { 'f','m','t',' ' });
    WriteLe32(wavOut, 16u);           // PCM fmt chunk length
    WriteLe16(wavOut, 1u);            // PCM format
    WriteLe16(wavOut, kChannels);
    WriteLe32(wavOut, kSampleRate);
    WriteLe32(wavOut, byteRate);
    WriteLe16(wavOut, blockAlign);
    WriteLe16(wavOut, kBitsPerSample);

    // data chunk
    wavOut.insert(wavOut.end(), { 'd','a','t','a' });
    WriteLe32(wavOut, dataSize);
    wavOut.insert(wavOut.end(), pcm.begin(), pcm.end());

    return true;
}


bool BsaArchive::LoadFromFile(const std::filesystem::path& filePath, BsaArchive& out, std::wstring* err) {
    out = {};
    out.sourcePath = filePath;

    if (!std::filesystem::exists(filePath)) {
        if (err) *err = L"File not found.";
        return false;
    }

    if (!ReadAllBytes(filePath, out.bytes, err)) {
        return false;
    }

    if (out.bytes.size() < 4) {
        if (err) *err = L"BSA is too small.";
        out = {};
        return false;
    }

    out.recordCount = ReadU16(out.bytes.data());
    out.recordType = ReadU16(out.bytes.data() + 2);

    size_t entrySize = 18;
    bool hasNames = true;
    if (out.recordType == 0x200) {
        entrySize = 8;
        hasNames = false;
    } else if (out.recordType != 0x100) {
        entrySize = 6;
        hasNames = false;
    }

    const size_t footerBytes = size_t(out.recordCount) * entrySize;
    if (out.bytes.size() < 4 + footerBytes) {
        if (err) *err = L"BSA footer is truncated.";
        out = {};
        return false;
    }

    const size_t footerStart = out.bytes.size() - footerBytes;
    size_t runningOffset = (out.recordType == 0x100 || out.recordType == 0x200) ? 4 : 2;

    out.entries.clear();
    out.entries.reserve(out.recordCount);

    for (size_t i = 0; i < out.recordCount; ++i) {
        size_t p = footerStart + i * entrySize;
        BsaEntry e{};
        e.offset = static_cast<uint32_t>(runningOffset);

        if (out.recordType == 0x100) {
            char nameBuf[13]{};
            memcpy(nameBuf, out.bytes.data() + p, 12);
            e.name = nameBuf;
            e.compressionFlag = ReadU16(out.bytes.data() + p + 12);
            e.packedSize = ReadU32(out.bytes.data() + p + 14);
        } else if (out.recordType == 0x200) {
            e.compressionFlag = ReadU16(out.bytes.data() + p + 0);
            uint16_t nameId = ReadU16(out.bytes.data() + p + 2);
            e.name = "REC_" + std::to_string(nameId);
            e.packedSize = ReadU32(out.bytes.data() + p + 4);
        } else {
            e.compressionFlag = 0;
            uint16_t nameId = ReadU16(out.bytes.data() + p + 0);
            e.name = "REC_" + std::to_string(nameId);
            e.packedSize = ReadU32(out.bytes.data() + p + 2);
        }

        if (e.packedSize > out.bytes.size() || runningOffset > out.bytes.size() - e.packedSize || runningOffset + e.packedSize > footerStart) {
            if (err) *err = L"BSA entry layout is invalid.";
            out = {};
            return false;
        }

        out.entries.push_back(std::move(e));
        runningOffset += out.entries.back().packedSize;
    }

    if (hasNames) {
        for (auto& e : out.entries) {
            while (!e.name.empty() && e.name.back() == '\0') e.name.pop_back();
        }
    }

    return true;
}

const BsaEntry* BsaArchive::FindEntryCaseInsensitive(std::string_view name) const {
    std::string needle(name);
    for (auto& c : needle) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));

    for (const auto& e : entries) {
        std::string n = e.name;
        for (auto& c : n) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        if (n == needle) return &e;
    }

    return nullptr;
}

bool BsaArchive::DecompressLzss(const uint8_t* data, size_t size, std::vector<uint8_t>& outBytes, std::wstring* err) {
    (void)err;
    outBytes.clear();
    if (!data || size == 0) return true;

    std::array<uint8_t, 4096> window{};
    for (size_t i = 0; i < 4078; ++i) window[i] = static_cast<uint8_t>(' ');

    size_t pos = 4078;
    size_t ip = 0;

    while (ip < size) {
        uint8_t flags = data[ip++];
        for (int bit = 0; bit < 8; ++bit) {
            if (ip >= size) return true;

            bool literal = ((flags >> bit) & 1u) != 0;
            if (literal) {
                uint8_t b = data[ip++];
                outBytes.push_back(b);
                window[pos] = b;
                pos = (pos + 1) & 0xFFFu;
            } else {
                if (ip + 1 >= size) return true;
                uint8_t b0 = data[ip++];
                uint8_t b1 = data[ip++];
                size_t offset = size_t(b0) | (size_t(b1 & 0xF0u) << 4);
                size_t length = size_t(b1 & 0x0Fu) + 3;
                for (size_t k = 0; k < length; ++k) {
                    uint8_t b = window[(offset + k) & 0xFFFu];
                    outBytes.push_back(b);
                    window[pos] = b;
                    pos = (pos + 1) & 0xFFFu;
                }
            }
        }
    }

    return true;
}

bool BsaArchive::ReadEntryData(const BsaEntry& entry, std::vector<uint8_t>& outBytes, std::wstring* err) const {
    outBytes.clear();

    if (entry.offset > bytes.size() || entry.packedSize > bytes.size() - entry.offset) {
        if (err) *err = L"BSA entry points outside archive bounds.";
        return false;
    }

    const uint8_t* payload = bytes.data() + entry.offset;
    const size_t payloadSize = entry.packedSize;

    if (entry.compressionFlag == 0) {
        outBytes.assign(payload, payload + payloadSize);
        return true;
    }

    return DecompressLzss(payload, payloadSize, outBytes, err);
}

bool FlcFile::NormalizeLeadingPrefix(std::vector<uint8_t>& bytes, bool& strippedPrefix) {
    strippedPrefix = false;

    // Typical FLC header has magic 0xAF12 at offset 4.
    auto hasFlcMagicAt = [&](size_t off) -> bool {
        if (off + 6 > bytes.size()) return false;
        return ReadU16(bytes.data() + off + 4) == 0xAF12;
    };

    if (hasFlcMagicAt(0)) return true;

    if (bytes.size() >= 8 && hasFlcMagicAt(2)) {
        bytes.erase(bytes.begin(), bytes.begin() + 2);
        strippedPrefix = true;
        return true;
    }

    return false;
}

bool FlcFile::LoadFromFile(const std::filesystem::path& filePath, FlcFile& out, std::wstring* err) {
    out = {};
    out.sourcePath = filePath;

    if (!std::filesystem::exists(filePath)) {
        if (err) *err = L"File not found.";
        return false;
    }

    if (!ReadAllBytes(filePath, out.bytes, err)) {
        return false;
    }

    bool stripped = false;
    if (!NormalizeLeadingPrefix(out.bytes, stripped)) {
        if (err) *err = L"File does not look like a Battlespire FLC payload (missing FLC magic).";
        out = {};
        return false;
    }

    out.hadLeadingUnknownPrefix = stripped;
    return true;
}

} // namespace battlespire
