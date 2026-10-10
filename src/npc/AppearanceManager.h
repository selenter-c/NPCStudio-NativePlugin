#pragma once
// TODO: Stage 4 — реализовать клонирование внешности NPC
// REF: см. RaceMenu/NiOverride API для работы с внешностью
namespace NPCStudio::NPC {
    class AppearanceManager {
    public:
        static bool CloneAppearance(RE::Actor* source, RE::Actor* target);
    };
}