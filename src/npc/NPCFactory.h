#pragma once
// TODO: Stage 3 — реализовать спавн NPC через PlaceAtMe
// TODO: Stage 4 — реализовать клонирование внешности
// TODO: Stage 5 — реализовать GetPlaceholderList
// REF: см. IntelEngine-NativePlugin/SKSE/src/ — примеры работы с акторами
namespace NPCStudio::NPC {
    class NPCFactory {
    public:
        static RE::Actor* SpawnBeforePlayer(RE::FormID placeholderFormID);
    };
}