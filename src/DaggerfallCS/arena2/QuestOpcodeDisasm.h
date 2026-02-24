#pragma once
#include "../pch.h"
#include "QuestQbn.h"
#include "TextRsc.h"

namespace arena2 {

struct OpCodeTypeInfo {
    uint16_t type{};
    const char* name{};
    const char* subSpec{};
};

const OpCodeTypeInfo* LookupOpCodeType(uint16_t type);

struct OpCodeDisasm {
    std::string typeName;
    std::string summary;
    std::string condition;
    std::vector<std::string> operands;
    uint16_t messageId{};
    std::string messagePreview;
};

OpCodeDisasm DisassembleOpCode(const QuestQbn& qbn, const QbnOpCodeRecord& rec, TextRsc* qrcOrNull);

bool SubRecordReferencesState(const QbnSubRecord& sr, uint16_t stateIndex);

} // namespace arena2
