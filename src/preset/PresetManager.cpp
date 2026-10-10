// TODO: Stage 10 — реализовать систему пресетов (сохранение/загрузка JSON)
// REF: см. nlohmann/json для работы с JSON
#include "preset/PresetManager.h"

namespace NPCStudio::Preset {

bool PresetManager::SavePreset(const std::string& name, const std::string& jsonData) {
    // TODO: Stage 10 — сохранение в Data/SKSE/Plugins/NPCStudio/Presets/
    return false;
}

std::string PresetManager::LoadPreset(const std::string& name) {
    // TODO: Stage 10 — загрузка из Data/SKSE/Plugins/NPCStudio/Presets/
    return "{}";
}

std::string PresetManager::ListPresets() {
    // TODO: Stage 10 — список пресетов
    return "[]";
}

} // namespace NPCStudio::Preset