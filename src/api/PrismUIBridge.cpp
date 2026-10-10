#include "api/PrismUIBridge.h"
#include <PrismaUI_API.h>
#include <spdlog/spdlog.h>
#include <array>
#include <functional>
#include <string>

namespace NPCStudio::API {

static constexpr size_t MAX_LISTENERS = 16;
static std::array<std::function<std::string(const std::string&)>, MAX_LISTENERS> s_callbacks;
static size_t s_callbackCount = 0;

template<size_t Index>
static void ListenerTrampoline(const char* argument) {
    if constexpr (Index < MAX_LISTENERS) {
        if (s_callbacks[Index]) {
            std::string arg = argument ? argument : "";
            s_callbacks[Index](arg);
        }
    }
}

using TrampolineFn = void(*)(const char*);

static const TrampolineFn s_trampolines[MAX_LISTENERS] = {
    ListenerTrampoline<0>,  ListenerTrampoline<1>,  ListenerTrampoline<2>,  ListenerTrampoline<3>,
    ListenerTrampoline<4>,  ListenerTrampoline<5>,  ListenerTrampoline<6>,  ListenerTrampoline<7>,
    ListenerTrampoline<8>,  ListenerTrampoline<9>,  ListenerTrampoline<10>, ListenerTrampoline<11>,
    ListenerTrampoline<12>, ListenerTrampoline<13>, ListenerTrampoline<14>, ListenerTrampoline<15>,
};

PrismUIBridge& PrismUIBridge::GetSingleton() {
    static PrismUIBridge instance;
    return instance;
}

bool PrismUIBridge::Initialize() {
    m_api = PRISMA_UI_API::RequestPluginAPI(PRISMA_UI_API::InterfaceVersion::V1);
    if (!m_api) {
        spdlog::warn("PrismUI API not available - UI will be disabled");
        return false;
    }
    spdlog::info("PrismUI API obtained successfully");
    return true;
}

void* PrismUIBridge::CreateView(const std::string& path) {
    if (!m_api) return nullptr;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI1*>(m_api);
    auto view = api->CreateView(path.c_str(), nullptr);
    if (view == 0) {
        spdlog::error("Failed to create PrismUI view: {}", path);
        return nullptr;
    }
    spdlog::info("PrismUI view created: {}", path);
    return reinterpret_cast<void*>(view);
}

void PrismUIBridge::ShowView(void* view, bool pauseGame) {
    if (!m_api || !view) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI1*>(m_api);
    auto v = reinterpret_cast<PrismaView>(view);
    api->Show(v);
    api->Focus(v, pauseGame, false);
}

void PrismUIBridge::HideView(void* view) {
    if (!m_api || !view) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI1*>(m_api);
    auto v = reinterpret_cast<PrismaView>(view);
    api->Unfocus(v);
    api->Hide(v);
}

void PrismUIBridge::RegisterJSListener(void* view, const std::string& name,
                                        std::function<std::string(const std::string&)> cb) {
    if (!m_api || !view) return;
    if (s_callbackCount >= MAX_LISTENERS) {
        spdlog::error("Too many JS listeners (max {})", MAX_LISTENERS);
        return;
    }

    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI1*>(m_api);
    size_t idx = s_callbackCount++;
    s_callbacks[idx] = std::move(cb);

    api->RegisterJSListener(
        reinterpret_cast<PrismaView>(view),
        name.c_str(),
        s_trampolines[idx]
    );
    spdlog::info("Registered JS listener: {} (slot {})", name, idx);
}

void PrismUIBridge::CallJS(void* view, const std::string& fn, const std::string& json) {
    if (!m_api || !view) return;
    auto* api = static_cast<PRISMA_UI_API::IVPrismaUI1*>(m_api);
    api->InteropCall(reinterpret_cast<PrismaView>(view), fn.c_str(), json.c_str());
}

bool PrismUIBridge::IsAvailable() const {
    return m_api != nullptr;
}

} // namespace NPCStudio::API