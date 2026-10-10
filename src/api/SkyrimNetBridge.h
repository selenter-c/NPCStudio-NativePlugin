#pragma once
// TODO: Stage 8 — реализовать обёртку над SkyrimNet Public API
// REF: см. IntelEngine-NativePlugin/SKSE/src/SkyrimNetAPI.h — паттерн обёртки
namespace NPCStudio::API {
    class SkyrimNetBridge {
    public:
        static SkyrimNetBridge& GetSingleton();
        bool Initialize();
        // TODO: Stage 8 — методы для работы с SkyrimNet
    private:
        SkyrimNetBridge() = default;
    };
}