#pragma once
#include "../pch.h"

namespace arena2 {

struct VarHashCatalog {
    // hash -> names (collisions possible)
    std::unordered_map<uint32_t, std::vector<std::string>> hashToNames;

    bool LoadFromFile(const std::filesystem::path& path, std::wstring* err);

    const std::vector<std::string>* NamesFor(uint32_t hash) const {
        auto it = hashToNames.find(hash);
        return (it == hashToNames.end()) ? nullptr : &it->second;
    }
};

uint32_t ComputeVarHash(std::string_view nameLowerAscii);

} // namespace arena2
