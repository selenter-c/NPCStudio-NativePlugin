#pragma once
// TODO: Stage 6 — реализовать управление экипировкой и инвентарём
// REF: см. IntelEngine-NativePlugin/SKSE/src/ — примеры работы с инвентарём
namespace NPCStudio::NPC {
    class InventoryManager {
    public:
        static bool EquipItem(RE::Actor* actor, RE::FormID itemFormID);
        static bool AddItem(RE::Actor* actor, RE::FormID itemFormID, int count);
    };
}