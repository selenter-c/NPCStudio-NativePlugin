#pragma once
#include <nlohmann/json.hpp>

namespace NPCStudio::Equipment {
nlohmann::json Describe(RE::Actor* actor, const nlohmann::json& command);
nlohmann::json Library();
void DeleteCustom(const std::string& action, const std::string& key);
void AddCategory(const std::string& name);
void SaveEditedPreset(const nlohmann::json& command);
void ValidatePreset(const std::string& id);
nlohmann::json Capture(RE::Actor* actor);
nlohmann::json Restore(RE::Actor* actor, const nlohmann::json& saved);
void Execute(RE::Actor* actor, const nlohmann::json& command);
}
