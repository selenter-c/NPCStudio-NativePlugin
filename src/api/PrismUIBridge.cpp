// TODO: Stage 2 — реализовать обёртку над PrismUI API
// REF: см. IntelEngine-NativePlugin/SKSE/src/PrismaUI_API.h — RequestPluginAPI
#include "api/PrismUIBridge.h"

namespace NPCStudio::API {

PrismUIBridge& PrismUIBridge::GetSingleton() {
    static PrismUIBridge instance;
    return instance;
}

bool PrismUIBridge::Initialize() {
    // TODO: Stage 2 — получение API через RequestPluginAPI
    return false;
}

void* PrismUIBridge::CreateView(const std::string& path) {
    // TODO: Stage 2 — создание view через IVPrismaUI1::CreateView
    return nullptr;
}

void PrismUIBridge::ShowView(void* view) {
    // TODO: Stage 2
}

void PrismUIBridge::HideView(void* view) {
    // TODO: Stage 2
}

void PrismUIBridge::RegisterJSListener(void* view, const std::string& name, 
                                        std::function<std::string(const std::string&)> cb) {
    // TODO: Stage 2
}

void PrismUIBridge::CallJS(void* view, const std::string& fn, const std::string& json) {
    // TODO: Stage 2
}

} // namespace NPCStudio::API