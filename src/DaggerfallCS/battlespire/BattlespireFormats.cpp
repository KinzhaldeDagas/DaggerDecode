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
