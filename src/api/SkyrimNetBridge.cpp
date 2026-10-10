// TODO: Stage 8 — реализовать обёртку над SkyrimNet Public API
// REF: см. IntelEngine-NativePlugin/SKSE/src/SkyrimNetAPI.h — паттерн LoadLibrary + GetProcAddress
#include "api/SkyrimNetBridge.h"

namespace NPCStudio::API {

SkyrimNetBridge& SkyrimNetBridge::GetSingleton() {
    static SkyrimNetBridge instance;
    return instance;
}

bool SkyrimNetBridge::Initialize() {
    // TODO: Stage 8 — загрузка SkyrimNet.dll через LoadLibraryA
    // TODO: Stage 8 — получение function pointers через GetProcAddress
    // TODO: Stage 8 — проверка версии через PublicGetVersion()
    return false;
}

} // namespace NPCStudio::API