#pragma once
#include <nlohmann/json.hpp>
namespace NPCStudio::Races {
nlohmann::json List();
RE::TESRace *Resolve(const std::string &editorId);
void Request(RE::Actor *actor, RE::TESRace *race);
} // namespace NPCStudio::Races
