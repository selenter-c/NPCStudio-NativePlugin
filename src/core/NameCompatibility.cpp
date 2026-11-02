#include "core/NameCompatibility.h"
#include "NND_API.h"

namespace NPCStudio::Names {
static NND_API::IVNND1* NND() {
    static auto* api = []() -> NND_API::IVNND1* {
        const auto module = GetModuleHandleW(L"NPCsNamesDistributor.dll");
        if (!module) return nullptr;
        const auto request = reinterpret_cast<NND_API::_RequestPluginAPI>(GetProcAddress(module, "RequestPluginAPI"));
        if (!request) return nullptr;
        auto* result = static_cast<NND_API::IVNND1*>(request(NND_API::InterfaceVersion::kV1));
        logger::info("Совместимость NND: {}", result ? "API подключён" : "API недоступен, используется SPID-патч");
        return result;
    }();
    return api;
}

void Protect(RE::TESNPC* base) {
    // Эти признаки касаются только нашего пула. Общие настройки NND не меняются.
    for (auto* keyword : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::BGSKeyword>()) {
        if (!keyword) continue;
        const auto* text = keyword->GetFormEditorID();
        if (!text) continue;
        const std::string_view id = text;
        if (id == "NNDUnique" || id == "NNDKnown" || id == "NNDDisableDefaultTitle" || id == "NNDDisableDefaultObscurity")
            base->AddKeyword(keyword);
    }
}

void Apply(RE::TESNPC* base, RE::Actor* actor, const std::string& name) {
    Protect(base);
    base->SetFullName(name.c_str());
    base->shortName = name;
    if (!actor->SetDisplayName(RE::BSFixedString(name), true))
        throw std::runtime_error("Не удалось установить имя персонажа");
    // NND хранит obscurity в собственном co-save: keyword недостаточно для старого кэша.
    if (auto* api = NND()) api->RevealName(actor);
    SKSE::GetTaskInterface()->AddUITask([] {
        if (auto* player = RE::PlayerCharacter::GetSingleton()) player->UpdateCrosshairs();
    });
}
}
