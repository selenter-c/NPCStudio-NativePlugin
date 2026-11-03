#pragma once
#include <nlohmann/json.hpp>
namespace NPCStudio::UI {
void Initialize();
void Close();
void PublishPreview(nlohmann::json update);
nlohmann::json Settings();
void SaveSettings(const nlohmann::json& settings);
}
