#pragma once
#include "../pch.h"

namespace battlespire {

struct RawScreenshot {
    static constexpr uint32_t kWidth = 640;
    static constexpr uint32_t kHeight = 480;
    static constexpr size_t kExpectedBytes = size_t(kWidth) * size_t(kHeight) * 3u;

    std::filesystem::path sourcePath;
    std::vector<uint8_t> rgb24;

    static bool LoadFromFile(const std::filesystem::path& filePath, RawScreenshot& out, std::wstring* err);
};

struct WavesPcm {
    // WAVES.BSA files contain unsigned 8-bit mono PCM at 11025hz.
    static constexpr uint16_t kChannels = 1;
    static constexpr uint16_t kBitsPerSample = 8;
    static constexpr uint32_t kSampleRate = 11025;

    static bool BuildWavFile(const std::vector<uint8_t>& pcm, std::vector<uint8_t>& wavOut, std::wstring* err);
};

struct FlcFile {
    std::filesystem::path sourcePath;
    std::vector<uint8_t> bytes;
    bool hadLeadingUnknownPrefix{ false };

    // Some Battlespire FLC files have two unknown bytes at the start.
    // If present and the payload appears to be an FLC afterwards, this strips those bytes.
    static bool LoadFromFile(const std::filesystem::path& filePath, FlcFile& out, std::wstring* err);

    static bool NormalizeLeadingPrefix(std::vector<uint8_t>& bytes, bool& strippedPrefix);
};

} // namespace battlespire
