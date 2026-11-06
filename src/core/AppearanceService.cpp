#include "core/AppearanceService.h"
#include <unordered_map>

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
    const auto* race = actor->GetRace();
    if (!part || part->IsDeleted() || part->type != Head::HeadPartType::kHair ||
        !part->flags.any(Head::Flag::kPlayable) || part->IsExtraPart() || part->formEditorID.empty() ||
        !part->GetModel() || !*part->GetModel() || !race || !part->validRaces || !part->validRaces->HasForm(race))
        return false;
    if (!part->GetFile(0) || (part->GetFormID() >> 24) == 0xFF) return false;
    return part->flags.any(actor->GetActorBase()->IsFemale() ? Head::Flag::kFemale : Head::Flag::kMale);
}

enum class MeshKind { Invalid, Static, Physics };
struct MeshInfo { MeshKind kind = MeshKind::Invalid; std::uint64_t fingerprint = 0; };

static MeshInfo InspectMesh(std::string path) {
    std::ranges::transform(path, path.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    std::ranges::replace(path, '/', '\\');
    if (path.empty() || path.starts_with('\\') || path.contains(':') || path.contains("..") || !path.ends_with(".nif"))
        return {};
    if (!path.starts_with("meshes\\")) path.insert(0, "meshes\\");
    static std::unordered_map<std::string, MeshInfo> cache;
    if (auto it = cache.find(path); it != cache.end()) return it->second;
    RE::BSResourceNiBinaryStream stream(path);
    if (!stream.good()) return {};
    const auto size = stream.stream->totalSize;
    if (size < 32 || size > 16 * 1024 * 1024) return {};
    std::string data(size, '\0');
    if (!stream.read(data.data(), size) || !data.starts_with("Gamebryo File Format"))
        return {};
    const auto kind = data.contains("HDT Skinned Mesh Physics Object") ? MeshKind::Physics : MeshKind::Static;
    const auto hash = std::hash<std::string>{}(data) ^ std::hash<std::string>{}(path);
    return cache.emplace(std::move(path), MeshInfo{kind, hash}).first->second;
}

static bool SafeMeshes(Head* part) {
    const auto valid = [](Head* element) {
        if (!element || element->IsDeleted() || !element->GetModel()) return false;
        const auto kind = InspectMesh(element->GetModel()).kind;
        return kind == MeshKind::Static || (kind == MeshKind::Physics && GetModuleHandleW(L"hdtSMP64.dll"));
    };
    return valid(part) && std::ranges::all_of(part->extraParts, [&](Head* extra) {
        return extra && extra->GetModel() && (!*extra->GetModel() || valid(extra));
    });
}

std::pair<RE::BGSHeadPart*, std::uint64_t> ThumbnailPart(RE::Actor* actor, RE::FormID id) {
    auto* part = RE::TESForm::LookupByID<Head>(id);
    if (!actor || !actor->GetActorBase() || !Compatible(actor, part))
        throw std::runtime_error("Модель причёски недоступна");
    // Снимок использует основной NIF. Дополнительные части проверяются при примерке.
    const auto mesh = InspectMesh(part->GetModel());
    if (mesh.kind == MeshKind::Invalid || (mesh.kind == MeshKind::Physics && !GetModuleHandleW(L"hdtSMP64.dll")))
        throw std::runtime_error("Модель причёски недоступна");
    return {part, mesh.fingerprint};
}
bool CanRestoreHair(RE::Actor* actor, Head* part) {
    return Compatible(actor, part) && SafeMeshes(part);
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
    for (auto* part : RE::TESDataHandler::GetSingleton()->GetFormArray<Head>()) {
        if (!Compatible(actor, part)) continue;
        auto row = Choice(part, HairName(part));
        row["plugin"] = part->GetFile()->GetFilename();
        hairs.push_back(std::move(row));
    }
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

void TryOn(RE::Actor* actor, bool managed, const json& saved, const json& command, Draft& draft) {
    if (!Ready(actor, managed)) throw std::runtime_error("Внешность этого NPC сейчас недоступна");
    if (!draft.target) {
        auto* current = BaseHair(actor);
        if (!managed) if (auto* previous = SavedHair(saved); HasMesh(actor, previous)) current = previous;
        auto* color = SavedColor(saved);
        if (!color) color = BaseColor(actor);
        draft = {actor->GetHandle(), current, current, color, color};
    }
    auto* hair = RE::TESForm::LookupByID<Head>(command.at("hair").get<RE::FormID>());
    const auto colorId = command.at("color").get<RE::FormID>();
    auto* color = colorId ? RE::TESForm::LookupByID<Color>(colorId) : nullptr;
    if (hair != draft.originalHair && (!Compatible(actor, hair) || !SafeMeshes(hair)))
        throw std::runtime_error("Причёска несовместима, отсутствует модель или требуется Faster HDT-SMP");
    if ((colorId && !color) || (color != draft.originalColor && !CompatibleColor(actor, color)))
        throw std::runtime_error("Цвет недоступен для этого NPC");
    if (color != draft.originalColor && !draft.originalColor)
        throw std::runtime_error("Исходный цвет волос недоступен");
    if (!HasMesh(actor, draft.hair) && HasMesh(actor, draft.originalHair)) draft.hair = draft.originalHair;
    if (!HasMesh(actor, draft.hair)) throw std::runtime_error("Снимите головной убор и дождитесь загрузки волос");
    if (hair != draft.hair) {
        auto* previous = draft.hair;
        draft.hair = hair;
        actor->ChangeHeadPart(previous, hair);
        if (!HasMesh(actor, hair)) throw std::runtime_error("Движок не подтвердил замену волос");
    }
    draft.color = color;
    Tint(actor, color);
}

json Commit(RE::Actor* actor, bool managed, const json& saved, Draft& draft) {
    auto result = saved;
    if (draft.hair != draft.originalHair) {
        if (managed) actor->GetActorBase()->ChangeHeadPart(draft.hair);
        result["hair"] = !managed && draft.hair == BaseHair(actor) ? json(nullptr) : Address(draft.hair);
    }
    if (draft.color != draft.originalColor) {
        if (managed) actor->GetActorBase()->SetHairColor(draft.color);
        result["color"] = Address(draft.color);
    }
    logger::info("Внешность NPC {:08X}: волосы {:08X} → {:08X}, цвет {:08X}, слот={}",
        actor->GetFormID(), draft.originalHair->GetFormID(), draft.hair->GetFormID(), draft.color ? draft.color->GetFormID() : 0, managed);
    draft = {};
    return result;
}

void Cancel(Draft& draft) {
    auto target = draft.target.get();
    auto* actor = target ? target->As<RE::Actor>() : nullptr;
    if (actor && actor->Is3DLoaded() && actor->GetFaceNode()) {
        if (draft.hair != draft.originalHair && HasMesh(actor, draft.hair) && !HasMesh(actor, draft.originalHair))
            actor->ChangeHeadPart(draft.hair, draft.originalHair);
        Tint(actor, draft.originalColor);
    }
    draft = {};
}

bool RestoreExternal(RE::Actor* actor, const json& saved) {
    if (!Ready(actor, false)) return false;
    auto* original = BaseHair(actor);
    auto* hair = SavedHair(saved);
    if (!CanRestoreHair(actor, hair)) hair = nullptr;
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
