#pragma once
#include <nlohmann/json.hpp>

namespace NPCStudio::Names {
nlohmann::json Random(bool female, std::string_view race);
void Protect(RE::TESNPC* base);
void Apply(RE::TESNPC* base, RE::Actor* actor, const std::string& name);
}
