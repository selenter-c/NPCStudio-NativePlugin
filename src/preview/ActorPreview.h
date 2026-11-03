#pragma once
#include <nlohmann/json.hpp>
namespace NPCStudio::Preview {
// Команды выполняются в игровом потоке; callback провайдера не обращается к RE.
nlohmann::json Frame(RE::Actor* actor, const std::string& identity, const nlohmann::json& options);
nlohmann::json ItemFrame(RE::TESBoundObject* item, const nlohmann::json& options);
bool IsCurrent(std::uint64_t generation);
void Clear();
void SavePortrait(const std::string& identity, const std::string& image);
} // namespace NPCStudio::Preview
