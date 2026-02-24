#include "pch.h"
#include <cstdio>
#include "QuestOpcodeDisasm.h"

namespace arena2 {

static const OpCodeTypeInfo kTypes[] = {
    { 0x00, "Item & Location", "3" },
    { 0x01, "Item & NPC", "3" },
    { 0x02, "Check Kill Count", "3" },
    { 0x03, "PC finds Item", "2" },
    { 0x04, "Items", "5" },
    { 0x05, "unknown", "3" },
    { 0x06, "States", "1" },
    { 0x07, "States", "2 or 5" },
    { 0x08, "Quest", "3" },
    { 0x09, "Repeating Spawn", "5" },
    { 0x0A, "Add Topics", "4" },
    { 0x0B, "Remove Topics", "4" },
    { 0x0C, "Start timer", "2" },
    { 0x0D, "Stop timer", "2" },
    { 0x11, "Locations", "4" },
    { 0x13, "Add Location to Map", "2" },
    { 0x15, "Mob hurt by PC", "2" },
    { 0x16, "Place Mob at Location", "3" },
    { 0x17, "Create Log Entry", "3" },
    { 0x18, "Remove Log", "2" },
    { 0x1A, "Give Item to NPC", "3" },
    { 0x1B, "Add Global Map", "4" },
    { 0x1C, "PC meets NPC", "2" },
    { 0x1D, "Yes/No Question", "4" },
    { 0x1E, "NPC, Location", "3" },
    { 0x1F, "Daily Clock", "3" },
    { 0x22, "Random State", "5" },
    { 0x23, "Cycle state", "5" },
    { 0x24, "Give Item to PC", "2" },
    { 0x25, "Item", "4" },
    { 0x26, "Rumors", "2" },
    { 0x27, "Item and Mob", "3" },
    { 0x2B, "PC at Location", "3" },
    { 0x2C, "Delete NPC", "2" },
    { 0x2E, "Hide NPC", "2" },
    { 0x30, "Show NPC", "2" },
    { 0x31, "Cure disease", "2" },
    { 0x32, "Play movie", "2" },
    { 0x33, "Display Message", "1" },
    { 0x34, "AND States", "5" },
    { 0x35, "OR States", "5" },
    { 0x36, "Make Item ordinary", "2" },
    { 0x37, "Escort NPC", "2" },
    { 0x38, "End Escort NPC", "2" },
    { 0x39, "Use Item", "3" },
    { 0x3A, "Cure Vampirism", "1" },
    { 0x3B, "Cure Lycanthropy", "1" },
    { 0x3C, "Play Sound", "2" },
    { 0x3D, "Reputation", "3" },
    { 0x3E, "Weather Override", "3" },
    { 0x3F, "Unknown", "2" },
    { 0x40, "Unknown", "2" },
    { 0x41, "Legal Reputation", "2" },
    { 0x44, "Mob", "3" },
    { 0x45, "Mob", "3" },
    { 0x46, "PC has Items", "5" },
    { 0x47, "Take PC Gold", "4" },
    { 0x48, "Unknown", "2" },
    { 0x49, "PC casts Spell", "3" },
    { 0x4C, "Give Item to PC", "2" },
    { 0x4D, "Check Player Level", "2" },
    { 0x4E, "Unknown", "2" },
    { 0x51, "Locations, NPC", "3" },
    { 0x52, "Stop NPC talk", "3" },
    { 0x53, "Location", "4" },
    { 0x54, "Play Sound", "4" },
    { 0x55, "Choose Questor", "4" },
    { 0x56, "End Questor", "4" },
    { 0x57, "Unknown", "5" },
};

const OpCodeTypeInfo* LookupOpCodeType(uint16_t type) {
    for (const auto& t : kTypes) {
        if (t.type == type) return &t;
    }
    return nullptr;
}

static const char* SectionName(uint16_t sec) {
    switch (sec) {
    case 0: return "Item";
    case 1: return "NPC";
    case 2: return "Location";
    case 3: return "Timer";
    case 4: return "Mob";
    case 8: return "OpCode";
    case 9: return "State";
    case 10: return "TextVar";
    default: return "Section";
    }
}

static std::string Hex32(uint32_t v) {
    char b[16]{};
    sprintf_s(b, "0x%08X", (unsigned)v);
    return std::string(b);
}

static std::string Hex16(uint16_t v) {
    char b[16]{};
    sprintf_s(b, "0x%04X", (unsigned)v);
    return std::string(b);
}

static void DecodeLocalPtr(uint32_t lp, uint16_t& sec, uint16_t& rec, bool& hasRec) {
    sec = (uint16_t)((lp >> 8) & 0xFF);
    rec = (uint16_t)(lp & 0xFF);
    hasRec = (rec != 0xFF);
}

bool SubRecordReferencesState(const QbnSubRecord& sr, uint16_t stateIndex) {
    uint16_t secLp=0, recLp=0; bool has=false;
    DecodeLocalPtr(sr.localPtr, secLp, recLp, has);
    if (sr.sectionId == 9 || secLp == 9) {
        if (has && recLp == stateIndex) return true;
        if (!has && (uint16_t)sr.value == stateIndex) return true;
    }
    return false;
}

static std::string FormatStateName(const QuestQbn& qbn, uint16_t stateIndex) {
    if (stateIndex < qbn.states.size() && !qbn.states[stateIndex].varNames.empty()) {
        return "_" + qbn.states[stateIndex].varNames[0] + "_";
    }
    return "State[" + std::to_string(stateIndex) + "]";
}

static std::string FormatSubRecord(const QuestQbn& qbn, const QbnSubRecord& sr) {
    // Constant pattern: localPtr == 0x12345678 and sectionId == 0
    if (sr.localPtr == 0x12345678 && sr.sectionId == 0) {
        return "Const(" + Hex32(sr.value) + ")";
    }

    uint16_t secLp=0, recLp=0; bool hasRec=false;
    DecodeLocalPtr(sr.localPtr, secLp, recLp, hasRec);

    uint16_t sec = sr.sectionId ? sr.sectionId : secLp;
    std::string s = std::string(SectionName(sec)) + "(";

    if (sec == 9) {
        uint16_t idx = hasRec ? recLp : (uint16_t)sr.value;
        s += FormatStateName(qbn, idx);
        s += ")";
        if (sr.notFlag == 1) s = "NOT " + s;
        return s;
    }

    // Generic: show section+record/value
    if (hasRec) {
        s += "rec=" + std::to_string(recLp);
        if (sr.value != 0xFFFFFFFF) s += " val=" + Hex32(sr.value);
    } else {
        if (sr.value != 0xFFFFFFFF) s += "val=" + Hex32(sr.value);
        else s += "none";
    }
    s += ")";
    if (sr.notFlag == 1) s = "NOT " + s;
    return s;
}

static std::string MessagePreview(TextRsc* qrc, uint16_t msgId) {
    if (!qrc) return std::string();
    if (msgId == 0xFFFF) return std::string();

    auto* rec = qrc->FindMutable(msgId);
    if (!rec) return std::string();
    rec->EnsureParsed(qrc->fileBytes);
    if (rec->subrecords.empty()) return std::string();

    auto& tt = rec->subrecords[0].EnsureTokens();
    std::string p = tt.plain;
    // keep short
    if (p.size() > 90) p = p.substr(0, 90) + "...";
    return p;
}

OpCodeDisasm DisassembleOpCode(const QuestQbn& qbn, const QbnOpCodeRecord& rec, TextRsc* qrcOrNull) {
    OpCodeDisasm d{};
    d.messageId = rec.messageId;

    if (auto* ti = LookupOpCodeType(rec.opCode)) {
        d.typeName = ti->name;
    } else {
        d.typeName = "Type " + Hex16(rec.opCode);
    }

    // Condition is always derived from first sub-record (state) in practice.
    d.condition = FormatSubRecord(qbn, rec.sub[0]);

    const uint16_t n = (rec.records > 5) ? 5 : rec.records;
    for (uint16_t i = 0; i < n; ++i) {
        d.operands.push_back(FormatSubRecord(qbn, rec.sub[i]));
    }

    d.messagePreview = MessagePreview(qrcOrNull, rec.messageId);

    // Summary line
    d.summary = d.typeName;
    d.summary += " | ";
    d.summary += d.condition;
    if (rec.messageId != 0xFFFF) {
        d.summary += " | Msg=" + Hex16(rec.messageId);
        if (!d.messagePreview.empty()) {
            d.summary += " \"" + d.messagePreview + "\"";
        }
    }
    return d;
}

} // namespace arena2
