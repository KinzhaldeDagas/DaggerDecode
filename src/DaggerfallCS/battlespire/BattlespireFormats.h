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


struct BsaEntry {
    std::string name;
    uint32_t offset{};
    uint32_t packedSize{};
    uint16_t compressionFlag{};
};

struct BsaArchive {
    std::filesystem::path sourcePath;
    uint16_t recordCount{};
    uint16_t recordType{};
    std::vector<uint8_t> bytes;
    std::vector<BsaEntry> entries;

    static bool LoadFromFile(const std::filesystem::path& filePath, BsaArchive& out, std::wstring* err);
    const BsaEntry* FindEntryCaseInsensitive(std::string_view name) const;
    bool ReadEntryData(const BsaEntry& entry, std::vector<uint8_t>& outBytes, std::wstring* err) const;

    // Tools/bsatool-compatible LZSS stream decode (used by pre-Morrowind BSA payloads).
    static bool DecompressLzss(const uint8_t* data, size_t size, std::vector<uint8_t>& outBytes, std::wstring* err);
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

struct Bs6ChunkInfo {
    std::string name;
    uint32_t length{};
};

struct Bs6FileSummary {
    bool valid{ false };
    std::vector<Bs6ChunkInfo> chunks;

    static bool TrySummarize(const std::vector<uint8_t>& bytes, Bs6FileSummary& out, std::wstring* err);
};


struct Int3 {
    int32_t x{};
    int32_t y{};
    int32_t z{};
};

struct Bs6SceneBox {
    std::array<Int3, 6> corners{};
};

struct Bs6ModelInstance {
    std::string modelName;
    Int3 position{};
    Int3 angles{};
    int32_t scale{ 1024 };
};

struct Bs6Scene {
    std::vector<Int3> markers;
    std::vector<Bs6SceneBox> boxes;
    std::vector<Bs6ModelInstance> models;
    std::vector<std::string> unresolvedModelNames;
    int32_t ambient = 0;
    int32_t brightness = 1023;
    uint32_t ambientSamples = 0;
    uint32_t brightnessSamples = 0;
    uint32_t litdCount = 0;
    uint32_t litsCount = 0;
    uint32_t fladCount = 0;
    uint32_t flasCount = 0;
    uint32_t rawdCount = 0;

    static bool TryBuildFromBytes(const std::vector<uint8_t>& bytes, Bs6Scene& out, std::wstring* err);
};

struct B3dFaceUv {
    int16_t u{};
    int16_t v{};
};

struct B3dFace {
    std::vector<uint32_t> pointIndices;
    std::vector<B3dFaceUv> uvs;
    std::array<uint8_t, 6> textureTag{};
};

struct B3dMesh {
    char version[5]{};
    std::vector<Int3> points;
    std::vector<B3dFace> faces;

    static bool TryParse(const std::vector<uint8_t>& bytes, B3dMesh& out, std::wstring* err);
};

struct B3dFileSummary {
    bool valid{ false };
    char version[5]{};
    uint32_t pointCount{};
    uint32_t planeCount{};
    uint32_t radius{};
    uint32_t objectCount{};
    uint32_t pointListOffset{};
    uint32_t normalListOffset{};
    uint32_t planeDataOffset{};
    uint32_t planeListOffset{};

    static bool TryParse(const std::vector<uint8_t>& bytes, B3dFileSummary& out, std::wstring* err);
};

} // namespace battlespire
