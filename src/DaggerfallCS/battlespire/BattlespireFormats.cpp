#include "pch.h"
#include "BattlespireFormats.h"
#include <functional>

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

bool Bs6FileSummary::TrySummarize(const std::vector<uint8_t>& bytes, Bs6FileSummary& out, std::wstring* err) {
    out = {};

    if (bytes.size() < 8) {
        if (err) *err = L"BS6 payload is too small.";
        return false;
    }

    size_t p = 0;
    while (p + 8 <= bytes.size()) {
        const uint8_t* h = bytes.data() + p;
        char tag[5]{};
        memcpy(tag, h, 4);
        uint32_t len = ReadU32(h + 4);

        if (len > bytes.size() - (p + 8)) {
            if (err) *err = L"BS6 chunk length points outside payload bounds.";
            return false;
        }

        Bs6ChunkInfo c{};
        c.name.assign(tag, tag + 4);
        c.length = len;
        out.chunks.push_back(std::move(c));

        p += 8 + size_t(len);
    }

    if (p != bytes.size()) {
        if (err) *err = L"BS6 payload has trailing bytes after final chunk.";
        return false;
    }

    out.valid = !out.chunks.empty();
    if (!out.valid) {
        if (err) *err = L"BS6 payload did not contain any chunks.";
        return false;
    }

    return true;
}


bool Bs6Scene::TryBuildFromBytes(const std::vector<uint8_t>& bytes, Bs6Scene& out, std::wstring* err) {
    out = {};

    auto readI32 = [](const uint8_t* p) -> int32_t {
        return static_cast<int32_t>(ReadU32(p));
    };

    auto normalizeModelName = [](std::string s) -> std::string {
        for (auto& c : s) if (c == '\\') c = '/';
        size_t slash = s.find_last_of('/');
        if (slash != std::string::npos) s = s.substr(slash + 1);

        auto isTrim = [](char ch) {
            return ch == '\0' || ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t';
        };
        while (!s.empty() && isTrim(s.back())) s.pop_back();
        while (!s.empty() && isTrim(s.front())) s.erase(s.begin());

        for (auto& c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        if (!s.empty() && s.find('.') == std::string::npos) s += ".3d";
        return s;
    };

    struct ObjTemplateList {
        std::vector<std::string> modelNames;
    };

    auto readChunkString = [&](const uint8_t* data, uint32_t len) -> std::string {
        if (!data || len == 0) return {};
        size_t n = 0;
        while (n < len && data[n] != '\0') ++n;
        if (n == 0) return {};
        return std::string(reinterpret_cast<const char*>(data), reinterpret_cast<const char*>(data) + n);
    };

    auto parseLfil = [&](const uint8_t* payload, uint32_t len, ObjTemplateList& outList) {
        if (len < 260) return;
        size_t count = len / 260;
        outList.modelNames.reserve(outList.modelNames.size() + count);
        for (size_t i = 0; i < count; ++i) {
            const uint8_t* src = payload + i * 260;
            std::string model = normalizeModelName(readChunkString(src, 260));
            if (!model.empty()) outList.modelNames.push_back(std::move(model));
        }
    };

    auto parseObjd = [&](const uint8_t* payload, uint32_t len, const ObjTemplateList* templates) {
        Bs6ModelInstance inst{};
        std::string dirName;
        std::string fileName;
        bool idfiOutOfRange = false;
        int32_t idfiValue = -1;

        auto resolveFromTemplates = [&](int32_t idx) -> std::string {
            if (!templates || templates->modelNames.empty()) return {};
            if (idx >= 0 && size_t(idx) < templates->modelNames.size()) {
                return normalizeModelName(templates->modelNames[size_t(idx)]);
            }
            // Some scene payloads appear to use 1-based template indices.
            if (idx > 0 && size_t(idx - 1) < templates->modelNames.size()) {
                return normalizeModelName(templates->modelNames[size_t(idx - 1)]);
            }
            return {};
        };

        std::function<void(const uint8_t*, size_t)> scanObjd;
        scanObjd = [&](const uint8_t* block, size_t blockLen) {
            size_t p = 0;
            while (p + 8 <= blockLen) {
                const uint8_t* h = block + p;
                std::string name(reinterpret_cast<const char*>(h), reinterpret_cast<const char*>(h) + 4);
                uint32_t clen = ReadU32(h + 4);
                size_t next = p + 8 + size_t(clen);
                if (next > blockLen) break;
                const uint8_t* c = block + p + 8;

                if (name == "IDFI" && clen >= 4) {
                    idfiValue = readI32(c);
                    std::string resolved = resolveFromTemplates(idfiValue);
                    if (!resolved.empty()) {
                        inst.modelName = std::move(resolved);
                    }
                    else {
                        idfiOutOfRange = true;
                    }
                }
                else if (name == "FILN") {
                    fileName = readChunkString(c, clen);
                }
                else if (name == "DIRN") {
                    dirName = readChunkString(c, clen);
                }
                else if (name == "POSI" && clen >= 12) {
                    inst.position = { readI32(c + 0), readI32(c + 4), readI32(c + 8) };
                }
                else if (name == "ANGS" && clen >= 12) {
                    inst.angles = { readI32(c + 0), readI32(c + 4), readI32(c + 8) };
                }
                else if (name == "SCAL" && clen >= 4) {
                    inst.scale = readI32(c);
                    if (inst.scale == 0) inst.scale = 1024;
                }

                if (name == "OBJD" || name == "OBJS") {
                    scanObjd(c, clen);
                }

                p = next;
            }
        };

        scanObjd(payload, len);

        if (inst.modelName.empty() && !fileName.empty()) {
            std::string joined = fileName;
            if (!dirName.empty()) joined = dirName + "/" + fileName;
            inst.modelName = normalizeModelName(joined);
        }

        if (!inst.modelName.empty()) {
            out.models.push_back(std::move(inst));
        }
        else if (idfiOutOfRange && idfiValue >= 0) {
            out.unresolvedModelNames.push_back("idfi:" + std::to_string(idfiValue));
        }
    };
    int64_t ambientSum = 0;
    int64_t brightnessSum = 0;

    if (bytes.size() < 8) {
        if (err) *err = L"BS6 scene payload is too small.";
        return false;
    }

    static const std::unordered_set<std::string> groupNames = {
        "GNRL", "TEXI", "STRU", "SNAP", "VIEW", "CTRL", "LINK", "OBJS", "OBJD", "LITS", "LITD", "FLAS", "FLAD"
    };

    static constexpr size_t kMaxChunkCount = 500000;
    static constexpr size_t kMaxWalkDepth = 64;
    size_t parsedChunkCount = 0;

    std::function<bool(size_t, size_t, ObjTemplateList*, size_t)> walk = [&](size_t start, size_t end, ObjTemplateList* inheritedTemplates, size_t depth) -> bool {
        if (depth > kMaxWalkDepth) {
            if (err) *err = L"BS6 scene parse exceeded maximum nested chunk depth.";
            return false;
        }

        size_t p = start;
        ObjTemplateList localTemplates = inheritedTemplates ? *inheritedTemplates : ObjTemplateList{};
        ObjTemplateList* activeTemplates = &localTemplates;

        while (p + 8 <= end) {
            if (++parsedChunkCount > kMaxChunkCount) {
                if (err) *err = L"BS6 scene parse exceeded maximum chunk count.";
                return false;
            }

            const uint8_t* h = bytes.data() + p;
            std::string name(reinterpret_cast<const char*>(h), reinterpret_cast<const char*>(h) + 4);
            uint32_t len = ReadU32(h + 4);
            size_t next = p + 8 + size_t(len);
            if (next > end) {
                if (err) *err = L"BS6 scene parse exceeded payload bounds.";
                return false;
            }
            if (next <= p) {
                if (err) *err = L"BS6 scene parse encountered non-advancing chunk.";
                return false;
            }

            const uint8_t* payload = bytes.data() + p + 8;
            if (name == "POSI" && len >= 12) {
                out.markers.push_back({ readI32(payload + 0), readI32(payload + 4), readI32(payload + 8) });
            }
            else if (name == "BBOX" && len >= 24) {
                Bs6SceneBox b{};
                if (len >= 72) {
                    for (size_t i = 0; i < 6; ++i) {
                        b.corners[i] = { readI32(payload + i * 12 + 0), readI32(payload + i * 12 + 4), readI32(payload + i * 12 + 8) };
                    }
                }
                else {
                    Int3 a{ readI32(payload + 0), readI32(payload + 4), readI32(payload + 8) };
                    Int3 c{ readI32(payload + 12), readI32(payload + 16), readI32(payload + 20) };
                    b.corners[0] = a;
                    b.corners[1] = { c.x, a.y, a.z };
                    b.corners[2] = { a.x, c.y, a.z };
                    b.corners[3] = { a.x, a.y, c.z };
                    b.corners[4] = c;
                    b.corners[5] = { a.x, c.y, c.z };
                }
                out.boxes.push_back(std::move(b));
            }
            else if (name == "AMBI" && len >= 4) {
                ambientSum += readI32(payload);
                out.ambientSamples++;
            }
            else if (name == "BRIT" && len >= 4) {
                brightnessSum += readI32(payload);
                out.brightnessSamples++;
            }
            else if (name == "LITD") {
                out.litdCount++;
            }
            else if (name == "LITS") {
                out.litsCount++;
            }
            else if (name == "FLAD") {
                out.fladCount++;
            }
            else if (name == "FLAS") {
                out.flasCount++;
            }
            else if (name == "RAWD") {
                out.rawdCount++;
            }
            else if (name == "LFIL") {
                parseLfil(payload, len, localTemplates);
                activeTemplates = &localTemplates;
            }
            else if (name == "OBJD") {
                parseObjd(payload, len, activeTemplates);
            }

            if (groupNames.find(name) != groupNames.end()) {
                ObjTemplateList* childTemplates = activeTemplates;
                if (name == "OBJS") childTemplates = &localTemplates;
                if (!walk(p + 8, next, childTemplates, depth + 1)) return false;
            }

            p = next;
        }

        if (p != end) {
            if (err) *err = L"BS6 scene parse encountered trailing bytes.";
            return false;
        }

        return true;
    };

    if (!walk(0, bytes.size(), nullptr, 0)) return false;

    if (!out.unresolvedModelNames.empty()) {
        std::sort(out.unresolvedModelNames.begin(), out.unresolvedModelNames.end());
        out.unresolvedModelNames.erase(std::unique(out.unresolvedModelNames.begin(), out.unresolvedModelNames.end()), out.unresolvedModelNames.end());
    }

    if (out.markers.empty() && out.boxes.empty() && out.models.empty()) {
        if (err) *err = L"BS6 scene contains no parseable markers, boxes, or models.";
        return false;
    }

    if (out.ambientSamples > 0) out.ambient = int32_t(ambientSum / (int64_t)out.ambientSamples);
    if (out.brightnessSamples > 0) out.brightness = int32_t(brightnessSum / (int64_t)out.brightnessSamples);

    // Phase-3 research observed AMBI in [0, 60000] and BRIT in [11, 1023] across scanned BS6 files.
    // Clamp to renderer-safe ranges to avoid malformed payloads producing pathological lighting multipliers.
    out.ambient = std::clamp(out.ambient, 0, 60000);
    out.brightness = std::clamp(out.brightness, 0, 1023);

    return true;
}

bool B3dMesh::TryParse(const std::vector<uint8_t>& bytes, B3dMesh& out, std::wstring* err) {
    out = {};

    if (bytes.size() < 64) {
        if (err) *err = L"3D payload is too small for mesh header.";
        return false;
    }

    uint32_t pointCount = ReadU32(bytes.data() + 4);
    uint32_t planeCount = ReadU32(bytes.data() + 8);
    uint32_t planeDataOffset = ReadU32(bytes.data() + 24);
    uint32_t pointListOffset = ReadU32(bytes.data() + 48);
    uint32_t planeListOffset = ReadU32(bytes.data() + 60);

    static constexpr uint32_t kMaxReasonablePoints = 1u << 20;
    static constexpr uint32_t kMaxReasonablePlanes = 1u << 20;
    if (pointCount > kMaxReasonablePoints || planeCount > kMaxReasonablePlanes) {
        if (err) *err = L"3D mesh counts are unreasonably large.";
        return false;
    }

    memcpy(out.version, bytes.data(), 4);
    out.version[4] = '\0';

    auto inBounds = [&](uint32_t off, size_t bytesNeeded) -> bool {
        return off <= bytes.size() && bytesNeeded <= bytes.size() - off;
    };
    if (!inBounds(pointListOffset, size_t(pointCount) * 12u) || !inBounds(planeDataOffset, size_t(planeCount) * 24u) || !inBounds(planeListOffset, 0)) {
        if (err) *err = L"3D mesh offsets are outside payload bounds.";
        return false;
    }

    out.points.reserve(pointCount);
    for (uint32_t i = 0; i < pointCount; ++i) {
        const uint8_t* p = bytes.data() + pointListOffset + size_t(i) * 12u;
        out.points.push_back({ static_cast<int32_t>(ReadU32(p + 0)), static_cast<int32_t>(ReadU32(p + 4)), static_cast<int32_t>(ReadU32(p + 8)) });
    }

    struct PlaneDataRef { std::array<uint8_t, 6> textureTag{}; };
    std::vector<PlaneDataRef> planeData;
    planeData.reserve(planeCount);
    for (uint32_t i = 0; i < planeCount; ++i) {
        const uint8_t* p = bytes.data() + planeDataOffset + size_t(i) * 24u;
        PlaneDataRef ref{};
        memcpy(ref.textureTag.data(), p + 4, 6);
        planeData.push_back(ref);
    }

    size_t planeCursor = planeListOffset;
    out.faces.reserve(planeCount);

    size_t discardedInvalidFaces = 0;
    for (uint32_t i = 0; i < planeCount; ++i) {
        if (planeCursor + 10 > bytes.size()) {
            if (err) *err = L"3D plane list truncated in header section.";
            return false;
        }

        uint8_t pointPerPlane = bytes[planeCursor + 0];
        static constexpr uint8_t kMaxPointsPerFace = 32;
        if (pointPerPlane == 0 || pointPerPlane > kMaxPointsPerFace) {
            if (err) *err = L"3D plane uses unsupported point count.";
            return false;
        }

        size_t pointsBytes = size_t(pointPerPlane) * 8u;
        size_t next = planeCursor + 10 + pointsBytes;
        if (next > bytes.size()) {
            if (err) *err = L"3D plane list truncated in point section.";
            return false;
        }

        B3dFace face{};
        face.textureTag = planeData[i].textureTag;
        face.pointIndices.reserve(pointPerPlane);
        face.uvs.reserve(pointPerPlane);

        bool invalidRef = false;
        for (size_t j = 0; j < pointPerPlane; ++j) {
            const uint8_t* pp = bytes.data() + planeCursor + 10 + j * 8u;
            uint32_t pointByteOffset = ReadU32(pp);
            if ((pointByteOffset % 12u) != 0u) {
                invalidRef = true;
                break;
            }
            uint32_t pointId = pointByteOffset / 12u;
            if (pointId >= out.points.size()) {
                invalidRef = true;
                break;
            }
            uint16_t u = static_cast<uint16_t>(pp[4] | (pp[5] << 8));
            uint16_t v = static_cast<uint16_t>(pp[6] | (pp[7] << 8));
            face.pointIndices.push_back(pointId);
            face.uvs.push_back({u, v});
        }

        if (!invalidRef && face.pointIndices.size() >= 3) {
            out.faces.push_back(std::move(face));
        } else {
            discardedInvalidFaces++;
        }

        planeCursor = next;
    }

    if (out.points.empty() || out.faces.empty()) {
        if (err) *err = L"3D mesh did not yield valid points/faces.";
        return false;
    }

    if (discardedInvalidFaces == planeCount) {
        if (err) *err = L"3D mesh discarded all faces due to invalid point references.";
        return false;
    }

    return true;
}

bool B3dFileSummary::TryParse(const std::vector<uint8_t>& bytes, B3dFileSummary& out, std::wstring* err) {
    out = {};

    // tools/3dtool uses a 64-byte header: <4s15I.
    if (bytes.size() < 64) {
        if (err) *err = L"3D payload is too small for header.";
        return false;
    }

    memcpy(out.version, bytes.data(), 4);
    out.version[4] = '\0';

    out.pointCount = ReadU32(bytes.data() + 4);
    out.planeCount = ReadU32(bytes.data() + 8);
    out.radius = ReadU32(bytes.data() + 12);
    out.planeDataOffset = ReadU32(bytes.data() + 24);
    out.objectCount = ReadU32(bytes.data() + 32);
    out.pointListOffset = ReadU32(bytes.data() + 48);
    out.normalListOffset = ReadU32(bytes.data() + 52);
    out.planeListOffset = ReadU32(bytes.data() + 60);

    auto inBounds = [&](uint32_t off, size_t bytesNeeded) -> bool {
        return off <= bytes.size() && bytesNeeded <= bytes.size() - off;
    };

    if (!inBounds(out.pointListOffset, size_t(out.pointCount) * 12u)) {
        if (err) *err = L"3D point list extends outside payload bounds.";
        return false;
    }

    // normals are 24 bytes per plane in tool reference.
    if (!inBounds(out.normalListOffset, size_t(out.planeCount) * 24u)) {
        if (err) *err = L"3D normal list extends outside payload bounds.";
        return false;
    }

    if (!inBounds(out.planeDataOffset, size_t(out.planeCount) * 24u)) {
        if (err) *err = L"3D plane data list extends outside payload bounds.";
        return false;
    }

    if (!inBounds(out.planeListOffset, 0)) {
        if (err) *err = L"3D plane list offset is outside payload bounds.";
        return false;
    }

    out.valid = true;
    return true;
}

} // namespace battlespire
