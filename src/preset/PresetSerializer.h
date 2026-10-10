#pragma once
// TODO: Stage 10 — реализовать сериализацию пресетов
// REF: см. nlohmann/json для работы с JSON
#include <string>

namespace NPCStudio::Preset {
    class PresetSerializer {
    public:
        static std::string SerializeNPC(RE::Actor* actor);
        static bool DeserializeToNPC(const std::string& json, RE::Actor* actor);
    };
}