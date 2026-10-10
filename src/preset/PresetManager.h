#pragma once
// TODO: Stage 10 — реализовать систему пресетов (сохранение/загрузка JSON)
// REF: см. nlohmann/json для работы с JSON
#include <string>

namespace NPCStudio::Preset {
    class PresetManager {
    public:
        static bool SavePreset(const std::string& name, const std::string& jsonData);
        static std::string LoadPreset(const std::string& name);
        static std::string ListPresets();
    };
}