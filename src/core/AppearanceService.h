#pragma once
#include <nlohmann/json.hpp>

namespace NPCStudio::Appearance {
nlohmann::json Describe(RE::Actor* actor, bool managed, const nlohmann::json& saved);
nlohmann::json Apply(RE::Actor* actor, bool managed, const nlohmann::json& saved, const nlohmann::json& command);
bool RestoreExternal(RE::Actor* actor, const nlohmann::json& saved);
RE::BGSColorForm* SavedColor(const nlohmann::json& saved);
RE::BGSHeadPart* SavedHair(const nlohmann::json& saved);
}
