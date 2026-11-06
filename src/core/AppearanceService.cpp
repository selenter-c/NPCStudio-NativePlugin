#include "core/AppearanceService.h"

namespace NPCStudio::Appearance {
using json = nlohmann::json;
using Head = RE::BGSHeadPart;
using Color = RE::BGSColorForm;

static json Address(const RE::TESForm* form) {
    const auto* file = form ? form->GetFile(0) : nullptr;
    if (!file || form->IsDeleted() || (form->GetFormID() >> 24) == 0xFF) return nullptr;
    return {{"plugin", file->GetFilename()}, {"form", form->GetLocalFormID()}};
}

template <class Form>
static Form* Resolve(const json& address) {
    if (address.is_null()) return nullptr;
    auto* form = RE::TESDataHandler::GetSingleton()->LookupForm<Form>(
        address.at("form").get<unsigned>(), address.at("plugin").get<std::string>());
    return form && !form->IsDeleted() ? form : nullptr;
}

static const RE::TESRace::FaceRelatedData* Face(RE::Actor* actor) {
    const auto* race = actor->GetRace();
    const auto sex = static_cast<unsigned>(actor->GetActorBase()->GetSex());
    return race && !race->IsChildRace() && sex < RE::SEXES::kTotal ? race->faceRelatedData[sex] : nullptr;
}

static Head* BaseHair(RE::Actor* actor) {
    auto* base = actor->GetActorBase();
    return base->HasOverlays() ? nullptr : base->GetHeadPartByType(Head::HeadPartType::kHair);
}

static bool Ready(RE::Actor* actor, bool managed) {
    if (!actor || actor->IsPlayerRef() || actor->IsDisabled() || actor->IsDead() || actor->IsInCombat() ||
        !actor->Is3DLoaded() || !actor->GetFaceNode()) return false;
    const auto* base = actor->GetActorBase();
    return base && (managed || (base->IsUnique() && actor->GetFile(0) && (actor->GetFormID() >> 24) != 0xFF)) &&
        Face(actor) && BaseHair(actor);
}

static bool HasMesh(RE::Actor* actor, const Head* part) {
    return part && !part->formEditorID.empty() && actor->GetFaceNode()->GetObjectByName(part->formEditorID);
}

static bool Compatible(RE::Actor* actor, Head* part) {
    if (!part || part->IsDeleted() || part->type != Head::HeadPartType::kHair ||
        !part->flags.any(Head::Flag::kPlayable) || part->IsExtraPart() || part->formEditorID.empty() ||
        !part->GetModel() || !*part->GetModel() || !part->validRaces || !part->validRaces->HasForm(actor->GetRace()))
        return false;
    const auto* file = part->GetFile();
    if (!file) return false;
    const std::string_view source = file->GetFilename();
    if (source != "Skyrim.esm" && source != "Update.esm" && source != "Dawnguard.esm" &&
        source != "HearthFires.esm" && source != "Dragonborn.esm") return false;
    return part->flags.any(actor->GetActorBase()->IsFemale() ? Head::Flag::kFemale : Head::Flag::kMale);
}

static json Choice(const RE::TESForm* form, std::string label) {
    const auto address = Address(form);
    if (address.is_null()) return nullptr;
    return {{"id", form->GetFormID()}, {"name", std::move(label)}};
}

static std::string HairName(const Head* part) {
    const auto* name = part->GetFullName();
    return name && *name ? name : part->GetFormEditorID();
}

static Color* BaseColor(RE::Actor* actor) {
    const auto* data = actor->GetActorBase()->headRelatedData;
    return data && data->hairColor ? data->hairColor : Face(actor)->defaultHairColor;
}

static bool CompatibleColor(RE::Actor* actor, const Color* color) {
    const auto* face = Face(actor);
    if (!color || Address(color).is_null()) return false;
    if (face->defaultHairColor == color) return true;
    return face->availableHairColors && std::ranges::find(*face->availableHairColors, color) != face->availableHairColors->end();
}

Color* SavedColor(const json& saved) {
    return Resolve<Color>(saved.value("color", json(nullptr)));
}
Head* SavedHair(const json& saved) {
    return Resolve<Head>(saved.value("hair", json(nullptr)));
}

static void Tint(RE::Actor* actor, Color* color) {
    if (color) {
        const auto& rgb = color->color;
        actor->GetFaceNode()->UpdateHairColor(RE::NiColor(rgb.red / 128.f, rgb.green / 128.f, rgb.blue / 128.f));
    }
}

json Describe(RE::Actor* actor, bool managed, const json& saved) {
    const auto* name = actor->GetDisplayFullName();
    json result = {{"name", name ? name : ""}, {"managed", managed}, {"available", Ready(actor, managed)}};
    if (!result["available"].get<bool>()) return result;
    auto* hair = BaseHair(actor);
    if (!managed) {
        auto* current = Resolve<Head>(saved.value("hair", json(nullptr)));
        if (HasMesh(actor, current)) hair = current;
    }
    auto* color = SavedColor(saved);
    if (!color) color = BaseColor(actor);
    result["hair"] = hair->GetFormID();
    result["hairName"] = HairName(hair);
    result["color"] = color ? color->GetFormID() : 0;
    result["hairAvailable"] = HasMesh(actor, hair);
    auto& hairs = result["hairs"] = json::array();
    for (auto* part : RE::TESDataHandler::GetSingleton()->GetFormArray<Head>())
        if (Compatible(actor, part)) hairs.push_back(Choice(part, HairName(part)));
    std::sort(hairs.begin(), hairs.end(), [](const json& left, const json& right) {
        return left.at("name").get_ref<const std::string&>() < right.at("name").get_ref<const std::string&>();
    });
    auto& colors = result["colors"] = json::array();
    const auto addColor = [&](Color* option) {
        if (Address(option).is_null()) return;
        const auto id = option->GetFormID();
        if (std::ranges::any_of(colors, [id](const json& row) { return row.at("id") == id; })) return;
        const auto& rgb = option->color;
        const auto hex = fmt::format("#{:02X}{:02X}{:02X}", rgb.red, rgb.green, rgb.blue);
        const auto* name = option->GetFullName();
        auto choice = Choice(option, name && *name ? name : hex);
        choice["rgb"] = hex;
        colors.push_back(std::move(choice));
    };
    addColor(color);
    addColor(Face(actor)->defaultHairColor);
    if (const auto* palette = Face(actor)->availableHairColors)
        for (auto* option : *palette) addColor(option);
    return result;
}

json Apply(RE::Actor* actor, bool managed, const json& saved, const json& command) {
    if (!Ready(actor, managed)) throw std::runtime_error("Внешность этого NPC сейчас недоступна");
    auto* original = BaseHair(actor);
    auto* current = original;
    if (!managed) {
        auto* previous = Resolve<Head>(saved.value("hair", json(nullptr)));
        if (HasMesh(actor, previous)) current = previous;
    }
    auto* hair = RE::TESForm::LookupByID<Head>(command.at("hair").get<RE::FormID>());
    const auto colorId = command.at("color").get<RE::FormID>();
    auto* color = colorId ? RE::TESForm::LookupByID<Color>(colorId) : nullptr;
    auto* currentColor = SavedColor(saved);
    if (!currentColor) currentColor = BaseColor(actor);
    if (hair != current && !Compatible(actor, hair)) throw std::runtime_error("Причёска несовместима с этим NPC");
    if ((colorId && !color) || (color != currentColor && !CompatibleColor(actor, color)))
        throw std::runtime_error("Цвет недоступен для этого NPC");
    if (!HasMesh(actor, current)) throw std::runtime_error("Снимите головной убор и дождитесь загрузки волос");
    auto result = saved;
    if (hair != current) {
        actor->ChangeHeadPart(current, hair);
        if (!HasMesh(actor, hair)) throw std::runtime_error("Движок не подтвердил замену волос");
        if (managed) actor->GetActorBase()->ChangeHeadPart(hair);
        result["hair"] = !managed && hair == original ? json(nullptr) : Address(hair);
    }
    if (color != currentColor) {
        if (managed) actor->GetActorBase()->SetHairColor(color);
        result["color"] = Address(color);
    }
    Tint(actor, color);
    logger::info("Внешность NPC {:08X}: волосы {:08X} → {:08X}, цвет {:08X}, слот={}",
        actor->GetFormID(), current->GetFormID(), hair->GetFormID(), colorId, managed);
    return result;
}

bool RestoreExternal(RE::Actor* actor, const json& saved) {
    if (!Ready(actor, false)) return false;
    auto* original = BaseHair(actor);
    auto* hair = SavedHair(saved);
    if (!Compatible(actor, hair)) hair = nullptr;
    if (!hair && !HasMesh(actor, original)) return false;
    if (hair && !HasMesh(actor, hair)) {
        if (!HasMesh(actor, original)) return false;
        actor->ChangeHeadPart(original, hair);
        if (!HasMesh(actor, hair))
            logger::warn("После загрузки NPC {:08X} движок не подтвердил замену волос", actor->GetFormID());
    }
    if (auto* color = SavedColor(saved); CompatibleColor(actor, color)) Tint(actor, color);
    return true;
}
}
