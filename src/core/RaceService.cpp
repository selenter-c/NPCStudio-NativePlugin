#include "core/RaceService.h"
namespace NPCStudio::Races {
static constexpr std::string_view Allowed[] = {"NordRace",     "BretonRace",  "ImperialRace", "RedguardRace",
                                               "HighElfRace",  "WoodElfRace", "DarkElfRace",  "OrcRace",
                                               "ArgonianRace", "KhajiitRace"};
RE::TESRace *Resolve(const std::string &editorId) {
    if (std::ranges::find(Allowed, editorId) == std::end(Allowed))
        throw std::runtime_error("Unsupported race");
    for (auto *race : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESRace>())
        if (race && editorId == race->GetFormEditorID() && !race->IsChildRace())
            return race;
    throw std::runtime_error("Race is unavailable");
}
nlohmann::json List() {
    auto result = nlohmann::json::array();
    for (auto id : Allowed) {
        try {
            auto *race = Resolve(std::string(id));
            result.push_back({{"id", id}, {"name", race->GetName()}});
        } catch (const std::exception &) {
        }
    }
    return result;
}
void SynchronizeSkin(RE::Actor *actor) {
    if (!actor || actor->IsPlayerRef() || !actor->Is3DLoaded()) return;
    auto* base = actor->GetActorBase();
    auto* race = actor->GetRace();
    if (!base || !race) return;
    const auto sex = static_cast<unsigned>(base->GetSex());
    if (sex >= RE::SEXES::kTotal) return;
    auto* face = race->faceRelatedData[sex];
    if (!face || !face->tintMasks) return;
    using Tone = RE::TESRace::FaceRelatedData::TintAsset::TintLayer::SkinTone;
    for (auto* asset : *face->tintMasks) {
        if (!asset || !asset->texture.skinTone.any(Tone::kSkinTone)) continue;
        // Голова уже пересобрана SwitchRace; переносим её слой кожи в QNAM тела.
        if (base->tintLayers) {
            for (auto* layer : *base->tintLayers) {
                if (layer && layer->tintIndex == asset->texture.index) {
                    base->bodyTintColor = layer->tintColor;
                    actor->UpdateSkinColor();
                    return;
                }
            }
        }
        if (asset->texture.presetDefault) {
            base->bodyTintColor = asset->texture.presetDefault->color;
            actor->UpdateSkinColor();
        }
        return;
    }
}
void Request(RE::Actor *actor, RE::TESRace *race) {
    if (!actor || !race || actor->IsPlayerRef())
        throw std::runtime_error("Race is unavailable");
    if (actor->GetRace() == race)
        return;
    if (actor->IsDisabled() || !actor->Is3DLoaded())
        throw std::runtime_error("NPC model is not loaded. Summon the character first");
    // Прямой метод движка из закреплённого CommonLib. Вызывается только на игровом потоке.
    logger::info("SwitchRace запрос для {:08X}: {}", actor->GetFormID(), race->GetFormEditorID());
    actor->SwitchRace(race, false);
}
} // namespace NPCStudio::Races
