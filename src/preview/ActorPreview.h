#pragma once
#include <nlohmann/json.hpp>
namespace NPCStudio::Preview {
// Все вызовы выполняются только в очереди игрового потока.
nlohmann::json Frame(RE::Actor *actor, const std::string &identity, const nlohmann::json &options);
void Clear();
} // namespace NPCStudio::Preview
