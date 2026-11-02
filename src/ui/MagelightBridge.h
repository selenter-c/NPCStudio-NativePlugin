#pragma once
#include <nlohmann/json.hpp>
namespace NPCStudio::UI {
void Initialize();
void Close();
nlohmann::json Settings();
void SaveSettings(const nlohmann::json& settings);
}
