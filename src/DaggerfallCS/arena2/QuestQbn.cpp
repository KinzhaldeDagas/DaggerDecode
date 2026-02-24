#include "pch.h"
#include "QuestQbn.h"

namespace arena2 {

static uint16_t ReadU16(const std::vector<uint8_t>& b, size_t off) {
    return (uint16_t)(b[off] | (uint16_t(b[off + 1]) << 8));
}
static int16_t ReadI16(const std::vector<uint8_t>& b, size_t off) {
    return (int16_t)ReadU16(b, off);
}
static uint32_t ReadU32(const std::vector<uint8_t>& b, size_t off) {
    return (uint32_t)b[off] | ((uint32_t)b[off + 1] << 8) | ((uint32_t)b[off + 2] << 16) | ((uint32_t)b[off + 3] << 24);
}

static bool ReadAllBytes(const std::filesystem::path& path, std::vector<uint8_t>& out, std::wstring* err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { if (err) *err = L"Failed to open QBN."; return false; }
    f.seekg(0, std::ios::end);
    auto sz = f.tellg();
    if (sz <= 0) { if (err) *err = L"QBN is empty."; return false; }
    f.seekg(0, std::ios::beg);
    out.resize((size_t)sz);
    f.read((char*)out.data(), sz);
    if (!f.good()) { if (err) *err = L"Failed to read QBN."; return false; }
    return true;
}

static std::string ReadCStr20(const std::vector<uint8_t>& b, size_t off) {
    char buf[21]{};
    for (int i = 0; i < 20; ++i) {
        uint8_t c = b[off + i];
        if (c == 0) break;
        buf[i] = (char)c;
    }
    return std::string(buf);
}

bool QuestQbn::LoadFromFile(const std::filesystem::path& path, const VarHashCatalog* hashes, std::wstring* err) {
    sourcePath = path;
    states.clear();
    textVars.clear();
    opcodes.clear();
    items.clear();
    npcs.clear();
    locations.clear();
    timers.clear();
    mobs.clear();
    itemByIndex.clear();
    npcByIndex.clear();
    locationByIndex.clear();
    timerByIndex.clear();
    mobByIndex.clear();
    header = {};

    if (!ReadAllBytes(path, fileBytes, err)) return false;
    const auto& b = fileBytes;

    // Header is fixed through byte 59.
    if (b.size() < 60) { if (err) *err = L"QBN too small."; return false; }

    header.questId = ReadU16(b, 0);
    header.factionId = ReadU16(b, 2);
    header.resourceId = ReadU16(b, 4);
    for (int i = 0; i < 9; ++i) header.resourceFilename[i] = b[6 + i];
    header.hasDebugInfo = b[15];

    size_t rcOff = 16;
    for (int i = 0; i < 10; ++i) header.sectionRecordCount[i] = ReadU16(b, rcOff + i * 2);

    size_t soOff = 36;
    for (int i = 0; i < 11; ++i) header.sectionOffset[i] = ReadU16(b, soOff + i * 2);

    // States section is index 9; each record 0x08 bytes per UESP.
    // [Bytes 0-1] FlagIndex (Int16), [2] IsGlobal, [3] GlobalIndex, [4-7] TextVariableHash. 
    
    // OpCodes section is index 8, 0x57 bytes per record.
    const uint16_t opcodeCount = header.sectionRecordCount[8];
    const uint16_t opcodeOff = header.sectionOffset[8];
    if (opcodeCount && opcodeOff) {
        size_t need = (size_t)opcodeOff + (size_t)opcodeCount * 0x57;
        if (need <= b.size()) {
            opcodes.reserve(opcodeCount);
            for (uint16_t i = 0; i < opcodeCount; ++i) {
                size_t o = (size_t)opcodeOff + (size_t)i * 0x57;
                QbnOpCodeRecord r{};
                r.fileOffset = (uint32_t)o;
                r.opCode = ReadU16(b, o + 0);
                r.flags = ReadU16(b, o + 2);
                r.records = ReadU16(b, o + 4);

                size_t so = o + 6;
                for (int si = 0; si < 5; ++si) {
                    QbnSubRecord s{};
                    s.notFlag = b[so + 0];
                    s.localPtr = ReadU32(b, so + 1);
                    s.sectionId = ReadU16(b, so + 5);
                    s.value = ReadU32(b, so + 7);
                    s.objectPtr = ReadU32(b, so + 11);
                    r.sub[si] = s;
                    so += 15;
                }

                r.messageId = ReadU16(b, o + 81);
                r.lastUpdate = ReadU32(b, o + 83);

                opcodes.push_back(std::move(r));
            }
        }
    }

const uint16_t stateCount = header.sectionRecordCount[9];
    const uint16_t stateOff = header.sectionOffset[9];

    if (stateCount && stateOff) {
        size_t need = (size_t)stateOff + (size_t)stateCount * 0x08;
        if (need <= b.size()) {
            states.reserve(stateCount);
            for (uint16_t i = 0; i < stateCount; ++i) {
                size_t o = (size_t)stateOff + (size_t)i * 0x08;
                QbnState s{};
                s.flagIndex = ReadI16(b, o + 0);
                s.isGlobal = b[o + 2];
                s.globalIndex = b[o + 3];
                s.textVarHash = ReadU32(b, o + 4);

                if (hashes) {
                    if (auto* names = hashes->NamesFor(s.textVarHash)) s.varNames = *names;
                }

                states.push_back(std::move(s));
            }
        }
    }

    
    // Items section (index 0), 0x13 bytes per record.
    const uint16_t itemCount = header.sectionRecordCount[0];
    const uint16_t itemOff = header.sectionOffset[0];
    if (itemCount && itemOff) {
        size_t need = (size_t)itemOff + (size_t)itemCount * 0x13;
        if (need <= b.size()) {
            items.reserve(itemCount);
            for (uint16_t i = 0; i < itemCount; ++i) {
                size_t o = (size_t)itemOff + (size_t)i * 0x13;
                QbnItem it{};
                it.itemIndex = ReadI16(b, o + 0);
                it.reward = b[o + 2];
                it.itemCategory = ReadU16(b, o + 3);
                it.itemCategoryIndex = ReadU16(b, o + 5);
                it.textVarHash = ReadU32(b, o + 7);
                it.textRecordId1 = ReadU16(b, o + 15);
                it.textRecordId2 = ReadU16(b, o + 17);

                if (hashes) {
                    if (auto* names = hashes->NamesFor(it.textVarHash)) it.varNames = *names;
                }
                items.push_back(std::move(it));
            }
        }
    }

    // NPCs section (index 3), 0x14 bytes per record.
    const uint16_t npcCount = header.sectionRecordCount[3];
    const uint16_t npcOff = header.sectionOffset[3];
    if (npcCount && npcOff) {
        size_t need = (size_t)npcOff + (size_t)npcCount * 0x14;
        if (need <= b.size()) {
            npcs.reserve(npcCount);
            for (uint16_t i = 0; i < npcCount; ++i) {
                size_t o = (size_t)npcOff + (size_t)i * 0x14;
                QbnNpc n{};
                n.npcIndex = ReadI16(b, o + 0);
                n.gender = b[o + 2];
                n.faceIndex = b[o + 3];
                n.unknown1 = ReadU16(b, o + 4);
                n.factionIndex = ReadU16(b, o + 6);
                n.textVarHash = ReadU32(b, o + 8);
                n.textRecordId1 = ReadU16(b, o + 16);
                n.textRecordId2 = ReadU16(b, o + 18);

                if (hashes) {
                    if (auto* names = hashes->NamesFor(n.textVarHash)) n.varNames = *names;
                }
                npcs.push_back(std::move(n));
            }
        }
    }

    // Locations section (index 4), 0x18 bytes per record.
    const uint16_t locCount = header.sectionRecordCount[4];
    const uint16_t locOff = header.sectionOffset[4];
    if (locCount && locOff) {
        size_t need = (size_t)locOff + (size_t)locCount * 0x18;
        if (need <= b.size()) {
            locations.reserve(locCount);
            for (uint16_t i = 0; i < locCount; ++i) {
                size_t o = (size_t)locOff + (size_t)i * 0x18;
                QbnLocation l{};
                l.locationIndex = ReadU16(b, o + 0);
                l.flags = b[o + 2];
                l.generalLocation = b[o + 3];
                l.fineLocation = ReadU16(b, o + 4);
                l.locationType = ReadI16(b, o + 6);
                l.doorSelector = ReadI16(b, o + 8);
                l.unknown2 = ReadU16(b, o + 10);
                l.textVarHash = ReadU32(b, o + 12);
                l.objPtr = ReadU32(b, o + 16);
                l.textRecordId1 = ReadU16(b, o + 20);
                l.textRecordId2 = ReadU16(b, o + 22);

                if (hashes) {
                    if (auto* names = hashes->NamesFor(l.textVarHash)) l.varNames = *names;
                }
                locations.push_back(std::move(l));
            }
        }
    }

    // Timers section (index 6), 0x21 bytes per record.
    const uint16_t timCount = header.sectionRecordCount[6];
    const uint16_t timOff = header.sectionOffset[6];
    if (timCount && timOff) {
        size_t need = (size_t)timOff + (size_t)timCount * 0x21;
        if (need <= b.size()) {
            timers.reserve(timCount);
            for (uint16_t i = 0; i < timCount; ++i) {
                size_t o = (size_t)timOff + (size_t)i * 0x21;
                QbnTimer t{};
                t.timerIndex = ReadI16(b, o + 0);
                t.flags = ReadU16(b, o + 2);
                t.type = b[o + 4];
                t.minimum = (int32_t)ReadU32(b, o + 5);
                t.maximum = (int32_t)ReadU32(b, o + 9);
                t.started = ReadU32(b, o + 13);
                t.duration = ReadU32(b, o + 17);
                t.link1 = (int32_t)ReadU32(b, o + 21);
                t.link2 = (int32_t)ReadU32(b, o + 25);
                t.textVarHash = ReadU32(b, o + 29);

                if (hashes) {
                    if (auto* names = hashes->NamesFor(t.textVarHash)) t.varNames = *names;
                }
                timers.push_back(std::move(t));
            }
        }
    }

    // Mobs section (index 7), 0x0e bytes per record.
    const uint16_t mobCount = header.sectionRecordCount[7];
    const uint16_t mobOff = header.sectionOffset[7];
    if (mobCount && mobOff) {
        size_t need = (size_t)mobOff + (size_t)mobCount * 0x0e;
        if (need <= b.size()) {
            mobs.reserve(mobCount);
            for (uint16_t i = 0; i < mobCount; ++i) {
                size_t o = (size_t)mobOff + (size_t)i * 0x0e;
                QbnMob m{};
                m.mobIndex = b[o + 0];
                m.null1 = ReadU16(b, o + 1);
                m.mobType = b[o + 3];
                m.mobCount = ReadU16(b, o + 4);
                m.textVarHash = ReadU32(b, o + 6);
                m.null2 = ReadU32(b, o + 10);

                if (hashes) {
                    if (auto* names = hashes->NamesFor(m.textVarHash)) m.varNames = *names;
                }
                mobs.push_back(std::move(m));
            }
        }
    }


// Optional Text Variables section (index 10) is present only for some QBNs; record size 0x1b and runs to EOF, terminated by empty string. 
    const uint16_t tvOff = header.sectionOffset[10];
    if (tvOff && tvOff < b.size()) {
        size_t o = tvOff;
        while (o + 0x1b <= b.size()) {
            QbnTextVariable tv{};
            tv.nameLower = ReadCStr20(b, o + 0);
            if (tv.nameLower.empty()) break;
            tv.sectionId = b[o + 20];
            tv.recordId = ReadU16(b, o + 21);
            tv.recordPtr = ReadU32(b, o + 23);

            tv.hash = ComputeVarHash(tv.nameLower);

            textVars.push_back(std::move(tv));
            o += 0x1b;
        }
    }

    // Enrich variable names using QBN textVars if hash matches (covers states + resources).
    if (!textVars.empty()) {
        std::unordered_map<uint32_t, std::string> hv;
        hv.reserve(textVars.size());
        for (const auto& tv : textVars) hv[tv.hash] = tv.nameLower;

        auto enrich = [&](uint32_t h, std::vector<std::string>& names) {
            auto it = hv.find(h);
            if (it == hv.end()) return;
            if (std::find(names.begin(), names.end(), it->second) == names.end())
                names.push_back(it->second);
        };

        for (auto& s : states) enrich(s.textVarHash, s.varNames);
        for (auto& it : items) enrich(it.textVarHash, it.varNames);
        for (auto& n : npcs) enrich(n.textVarHash, n.varNames);
        for (auto& l : locations) enrich(l.textVarHash, l.varNames);
        for (auto& t : timers) enrich(t.textVarHash, t.varNames);
        for (auto& mb : mobs) enrich(mb.textVarHash, mb.varNames);
    }

    BuildIndexMaps();


    return true;
}

} // namespace arena2
