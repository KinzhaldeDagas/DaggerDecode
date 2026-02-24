#include "pch.h"
#include "QuestCatalog.h"

namespace arena2 {

static inline char Up(char c) { return (char)toupper((unsigned char)c); }

const char* QuestCatalog::GuildNameForCode(char c) {
    switch (Up(c)) {
        case '_': return "Starting Quests";
        case '$': return "Cure Quests";
        case '0': return "School of Julianos";
        case '1': return "Meridia";
        case '2': return "Molag Bal";
        case '3': return "Namira";
        case '4': return "Nocturnal";
        case '5': return "Peryite";
        case '6': return "Sheogorath";
        case '7': return "Sanguine";
        case '8': return "Malacath";
        case '9': return "Vaermina";
        case 'A': return "Commoners";
        case 'B': return "Knightly Orders";
        case 'C': return "Temple Quests";
        case 'D': return "Akatosh Chantry";
        case 'E': return "Temple of Arkay";
        case 'F': return "House of Dibella";
        case 'G': return "Kynaran Order";
        case 'H': return "Benevolence of Mara";
        case 'I': return "Temple of Stendarr";
        case 'J': return "Resolution of Zenithar";
        case 'K': return "Merchants";
        case 'L': return "Dark Brotherhood";
        case 'M': return "Fighters Guild";
        case 'N': return "Mages Guild";
        case 'O': return "Thieves Guild";
        case 'P': return "Vampires";
        case 'Q': return "Covens";
        case 'R': return "Royalty";
        case 'S': return "Main Quests";
        case 'T': return "Azura (Crimson Gate)";
        case 'U': return "Boethiah";
        case 'V': return "Clavicus Vile";
        case 'W': return "Hermaeus Mora";
        case 'X': return "Hircine";
        case 'Y': return "Mehrunes Dagon";
        case 'Z': return "Mephala";
        default:  return "Unknown";
    }
}

const char* QuestCatalog::MembershipNameForCode(char c) {
    switch (Up(c)) {
        case 'A': return "Join";
        case 'B': return "Member";
        case 'C': return "Non-member";
        default:  return "Unknown";
    }
}

const char* QuestCatalog::DeliveryNameForCode(char c) {
    switch (Up(c)) {
        case 'Y': return "In-person";
        case 'L': return "Letter";
        default:  return "Unknown";
    }
}

bool QuestCatalog::IsStandardQuestFilename(const std::string& baseName) {
    // Standard format: [0]=guild, [1]='0', [2]=membership, [3]=minrep digit, [4]=childguard, [5]=delivery, [6-7]=suffix
    if (baseName.size() != 8) return false;
    if (baseName[1] != '0') return false;
    return true;
}

static std::string UpperStem(const std::filesystem::path& p) {
    auto s = p.stem().string();
    for (auto& c : s) c = (char)toupper((unsigned char)c);
    return s;
}


static std::string SanitizeForTitle(std::string s) {
    auto issp = [](unsigned char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; };
    // Drop %var and _var_ tokens (cheap sanitizer)
    std::string out;
    out.reserve(s.size());
    for (size_t i=0;i<s.size();++i) {
        char c = s[i];
        if (c=='%') {
            size_t j=i+1;
            while (j<s.size()) {
                unsigned char d=(unsigned char)s[j];
                bool ok=(d>='a'&&d<='z')||(d>='A'&&d<='Z')||(d>='0'&&d<='9')||d=='_';
                if (!ok) break;
                j++;
            }
            if (j>i+1) { i=j-1; continue; }
        }
        if (c=='_') {
            size_t j=i+1;
            while (j<s.size()) {
                unsigned char d=(unsigned char)s[j];
                bool ok=(d>='a'&&d<='z')||(d>='A'&&d<='Z')||(d>='0'&&d<='9')||d=='_';
                if (!ok) break;
                j++;
            }
            if (j<s.size() && s[j]=='_' && j>i+1) { i=j; continue; }
        }
        out.push_back(c);
    }
    s = std::move(out);

    // Normalize whitespace
    out.clear();
    bool prevSpace=false;
    for (char c : s) {
        unsigned char d=(unsigned char)c;
        if (issp(d)) {
            if (!prevSpace) out.push_back(' ');
            prevSpace=true;
        } else {
            out.push_back(c);
            prevSpace=false;
        }
    }
    while (!out.empty() && out.front()==' ') out.erase(out.begin());
    while (!out.empty() && out.back()==' ') out.pop_back();
    return out;
}

static std::string FirstSentenceOrLine(const std::string& s) {
    // First line
    size_t eol = s.find_first_of("\r\n");
    std::string line = (eol==std::string::npos) ? s : s.substr(0,eol);

    // First sentence within line
    size_t p = line.find_first_of(".!?");
    if (p != std::string::npos && p >= 10) {
        return line.substr(0, p+1);
    }
    return line;
}

static void DeriveQuestDisplayName(QuestEntry& q) {
    q.displayName.clear();
    q.displayNameSourceRecord = 0;

    if (!q.qrcLoaded) return;
    if (q.qrc.records.empty()) return;

    auto pickFromRecord = [&](uint16_t recId) -> bool {
        auto* rec = q.qrc.FindMutable(recId);
        if (!rec) return false;
        rec->EnsureParsed(q.qrc.fileBytes);
        if (rec->subrecords.empty()) return false;

        auto& tt = rec->subrecords[0].EnsureTokens();
        std::string t = SanitizeForTitle(tt.plain);
        t = FirstSentenceOrLine(t);
        if (t.size() < 6) return false;

        // Avoid pure boilerplate
        if (t == "..." || t == "?" || t == "!") return false;

        // Clamp to sane UI length
        if (t.size() > 88) {
            t = t.substr(0, 85);
            t += "...";
        }

        q.displayName = t;
        q.displayNameSourceRecord = recId;
        return true;
    };

    // Preferred: derive from the first "Create Log Entry" pseudo-code (opCode 0x0017),
    // which references the log text via Sub-record 2 (a message ID).
    for (const auto& op : q.qbn.opcodes) {
        if (op.opCode != 0x0017) continue;
        if ((int)op.records < 3) continue;

        uint32_t msg32 = op.sub[1].value;
        if (msg32 > 0xFFFFu) continue;

        uint16_t msgRecId = (uint16_t)msg32;
        if (msgRecId == 0xFFFFu) continue;

        if (pickFromRecord(msgRecId)) return;
    }

    // Fallback order: log entry then offer/accept/decline/completion.
    const uint16_t candidates[] = { 0x03F2, 0x03E8, 0x03EC, 0x03EA, 0x03E9 };
    for (uint16_t recId : candidates) {
        if (pickFromRecord(recId)) return;
    }

    // Final fallback: first record's first subrecord.
    auto& r0 = q.qrc.records[0];
    r0.EnsureParsed(q.qrc.fileBytes);
    if (!r0.subrecords.empty()) {
        auto& tt = r0.subrecords[0].EnsureTokens();
        std::string t = SanitizeForTitle(tt.plain);
        t = FirstSentenceOrLine(t);
        if (t.size() > 88) { t = t.substr(0, 85); t += "..."; }
        if (!t.empty()) {
            q.displayName = t;
            q.displayNameSourceRecord = r0.recordId;
        }
    }
}

static void ParseFilenameMeta(QuestEntry& q) {
    q.standardFilename = QuestCatalog::IsStandardQuestFilename(q.baseName);

    // Spec is primarily for standard quest filenames; special prefixes ($, _, S, etc.) exist.
    // We follow the bytes described in Quest Filename Format where applicable. 
    if (q.baseName.size() < 6) return;

    q.guildCode = q.baseName[0];
    q.membershipCode = q.baseName[2];
    q.minRepCode = q.baseName[3];
    q.childGuardCode = q.baseName[4];
    q.deliveryCode = q.baseName[5];
    q.guildName = QuestCatalog::GuildNameForCode(q.guildCode);
    q.membershipName = q.standardFilename ? QuestCatalog::MembershipNameForCode(q.membershipCode) : "Unknown";
    q.deliveryName = q.standardFilename ? QuestCatalog::DeliveryNameForCode(q.deliveryCode) : "Unknown";

    q.childGuardRestricted = (q.standardFilename && (q.childGuardCode == 'X' || q.childGuardCode == 'Y'));

    if (q.standardFilename && q.minRepCode >= '0' && q.minRepCode <= '9') {
        q.minRepValue = int(q.minRepCode - '0') * 10;
    } else {
        q.minRepValue = -1;
    }

}

bool QuestCatalog::LoadFromArena2Root(const std::filesystem::path& folder, const VarHashCatalog* hashes, std::wstring* err) {
    quests.clear();
	this->hashes = hashes;

    std::filesystem::path root = folder;
    std::filesystem::path arena2 = root / "ARENA2";
    if (std::filesystem::exists(arena2) && std::filesystem::is_directory(arena2)) root = arena2;
    arena2Root = root;

    // Scan for *.QBN in ARENA2 root.
    std::vector<std::filesystem::path> qbnFiles;
    for (auto& it : std::filesystem::directory_iterator(root)) {
        if (!it.is_regular_file()) continue;
        auto ext = it.path().extension().string();
        for (auto& c : ext) c = (char)toupper((unsigned char)c);
        if (ext == ".QBN") qbnFiles.push_back(it.path());
    }

    std::sort(qbnFiles.begin(), qbnFiles.end(), [](const auto& a, const auto& b) { return UpperStem(a) < UpperStem(b); });

    quests.reserve(qbnFiles.size());
    for (const auto& qbn : qbnFiles) {
        QuestEntry e{};
        e.baseName = UpperStem(qbn);
        e.qbnPath = qbn;
        e.qrcPath = qbn;
        e.qrcPath.replace_extension(".QRC");
        ParseFilenameMeta(e);

        std::wstring perr;
		e.qbnLoaded = e.qbn.LoadFromFile(e.qbnPath, hashes, &perr); // Non-fatal: allow listing even if parse fails
        quests.push_back(std::move(e));
    }

    if (quests.empty()) {
        if (err) *err = L"No QBN files found in ARENA2.";
        return false;
    }

    return true;
}

bool QuestCatalog::EnsureQrcLoaded(size_t questIndex, std::wstring* err) {
    if (questIndex >= quests.size()) return false;
    auto& q = quests[questIndex];
    if (q.qrcLoaded) return true;

    if (!std::filesystem::exists(q.qrcPath)) {
        if (err) *err = L"Missing matching QRC file for this quest.";
        return false;
    }

    TextRsc t{};
    if (!TextRsc::LoadFromFile(q.qrcPath, t, err)) return false;
    q.qrc = std::move(t);
    q.qrcLoaded = true;
    DeriveQuestDisplayName(q);
    return true;
}

} // namespace arena2
