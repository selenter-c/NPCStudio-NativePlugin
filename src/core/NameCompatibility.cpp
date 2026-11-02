#include "core/NameCompatibility.h"
#include "core/StateCodec.h"
#include "NND_API.h"
#include <fstream>
#include <random>

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

nlohmann::json Random(bool female, std::string_view race) {
    using json = nlohmann::json;
    std::string stem = "NNDNord";
    for (auto [token, file] : {std::pair{"WoodElf", "NNDBosmer"}, {"HighElf", "NNDAltmer"}, {"DarkElf", "NNDDunmer"},
        {"Breton", "NNDBreton"}, {"Imperial", "NNDImperial"}, {"Redguard", "NNDRedguard"}, {"Orc", "NNDOrc"},
        {"Argonian", "NNDArgonianTamrielic"}, {"Khajiit", "NNDKhajiitTaagraPrefixHonorifics"}})
        if (race.find(token) != std::string_view::npos) { stem = file; break; }
    const auto path = std::filesystem::path("Data/SKSE/Plugins/NPCsNamesDistributor") / (stem + ".json");
    std::vector<std::string> names;
    std::string source = "Встроенные имена NPCStudio";
    try {
        if (std::filesystem::exists(path) && std::filesystem::file_size(path) < 4 * 1024 * 1024) {
            std::ifstream file(path); auto definition = json::parse(file);
            for (const auto* segment : {"First", "Middle"}) {
                if (!definition.contains(segment)) continue;
                const auto& value = definition.at(segment);
                for (const auto* gender : {female ? "Female" : "Male", "Any"}) {
                    if (!value.contains(gender) || !value.at(gender).contains("Names")) continue;
                    for (const auto& name : value.at(gender).at("Names"))
                        if (name.is_string() && ValidName(name.get<std::string>())) names.push_back(name.get<std::string>());
                }
                if (!names.empty()) break;
            }
            if (!names.empty()) source = stem + ".json (Tamrielic Names / NND)";
        }
    } catch (const std::exception& e) { logger::warn("Имена {}: {}", stem, e.what()); }
    if (names.empty()) names = female ? std::vector<std::string>{"Эйра", "Хельга", "Сигрид", "Руна", "Фрейда", "Ингрид"} :
        std::vector<std::string>{"Эрандир", "Фаэлин", "Андорил", "Галедор", "Рален", "Фенрил"};
    static std::mt19937 generator(std::random_device{}());
    return {{"name", names.at(std::uniform_int_distribution<std::size_t>(0, names.size() - 1)(generator))}, {"source", source}};
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
