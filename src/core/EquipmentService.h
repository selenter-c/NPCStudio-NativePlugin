#pragma once
#include <nlohmann/json.hpp>

namespace NPCStudio::Equipment {
nlohmann::json Describe(RE::Actor* actor, const nlohmann::json& command);
void Execute(RE::Actor* actor, const nlohmann::json& command);
}
