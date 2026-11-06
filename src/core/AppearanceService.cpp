#include "core/AppearanceService.h"
#include <cmath>
#include <unordered_map>

namespace NPCStudio::Appearance {
using json = nlohmann::json;
using Head = RE::BGSHeadPart;
using PartType = Head::HeadPartType;
using Color = RE::BGSColorForm;
struct FacePart {
    const char* key;
    PartType type;
};
constexpr std::array FaceParts{
    FacePart{"eyes", PartType::kEyes}, FacePart{"brows", PartType::kEyebrows}, FacePart{"beard", PartType::kFacialHair}};
constexpr int MinHeightPercent = 75;
constexpr int MaxHeightPercent = 125;

static json Address(const RE::TESForm* form) {
    const auto* file = form ? form->GetFile(0) : nullptr;
    if (!file || form->IsDeleted() || (form->GetFormID() >> 24) == 0xFF) return nullptr;
    return {{"plugin", file->GetFilename()}, {"form", form->GetLocalFormID()}};
}

template <class Form>
static Form* Resolve(const json& address) {
    if (!address.is_object() || !address.contains("plugin") || !address["plugin"].is_string() ||
        !address.contains("form") || !address["form"].is_number_unsigned()) return nullptr;
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

json Height(RE::Actor* actor) {
    return {{"percent", actor->GetReferenceRuntimeData().refScale}, {"min", MinHeightPercent}, {"max", MaxHeightPercent}};
}

static bool Ready(RE::Actor* actor, bool managed) {
    if (!actor || actor->IsPlayerRef() || actor->IsDisabled() || actor->IsDead() || actor->IsInCombat() ||
        !actor->Is3DLoaded() || !actor->GetFaceNode()) return false;
    const auto* base = actor->GetActorBase();
    return base && (managed || (base->IsUnique() && actor->GetFile(0) && (actor->GetFormID() >> 24) != 0xFF)) &&
        Face(actor) && BaseHair(actor);
}

void SetHeight(RE::Actor* actor, bool managed, const json& percent) {
    if (!Ready(actor, managed)) throw std::runtime_error("Внешность этого NPC сейчас недоступна");
    if (!percent.is_number_integer() || percent < MinHeightPercent || percent > MaxHeightPercent)
        throw std::runtime_error("Рост должен быть от 75 до 125%");
    const auto value = percent.get<int>();
    if (actor->GetReferenceRuntimeData().refScale == value) return;
    // Убираем погрешность float перед преобразованием движка в целые сотые.
    actor->SetScale(std::nextafter(value / 100.f, 2.f));
    actor->AddChange(RE::TESObjectREFR::ChangeFlags::kScale);
    if (actor->GetReferenceRuntimeData().refScale != value)
        throw std::runtime_error("Движок не подтвердил изменение роста");
}

static bool HasMesh(RE::Actor* actor, const Head* part) {
    const auto* name = part ? part->formEditorID.c_str() : nullptr;
    auto* face = actor->GetFaceNode();
    return name && *name && face && face->GetObjectByName(part->formEditorID);
}

Head* VisualHair(RE::Actor* actor, const json& saved) {
    if (auto* hair = SavedHair(saved); HasMesh(actor, hair)) return hair;
    return BaseHair(actor);
}

bool HairAwaitingBones(RE::Actor* actor, Head* hair) {
    auto* face = actor->GetFaceNode();
    if (!face || !hair) return false;
    const auto unbound = [&](Head* part) {
        if (!part || part->formEditorID.empty()) return false;
        auto* node = face->GetObjectByName(part->formEditorID);
        if (!node) return false;
        return RE::BSVisit::TraverseScenegraphGeometries(node, [](RE::BSGeometry* shape) {
            const auto& data = shape->GetGeometryRuntimeData();
            const bool waiting = data.skinInstance ? !data.skinInstance->bones :
                shape->AsDynamicTriShape() || data.vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_SKINNED);
            return waiting ? RE::BSVisit::BSVisitControl::kStop : RE::BSVisit::BSVisitControl::kContinue;
        }) == RE::BSVisit::BSVisitControl::kStop;
    };
    return unbound(hair) || std::ranges::any_of(hair->extraParts, unbound);
}

static bool BlankBeard(const Head* part) {
    return part && part->type == PartType::kFacialHair && (!part->GetModel() || !*part->GetModel()) && part->extraParts.empty();
}

static bool Compatible(RE::Actor* actor, Head* part, PartType type = PartType::kHair) {
    const auto* race = actor->GetRace();
    if (!part || part->IsDeleted() || part->type != type ||
        !part->flags.any(Head::Flag::kPlayable) || part->IsExtraPart() || part->formEditorID.empty() ||
        (!BlankBeard(part) && (!part->GetModel() || !*part->GetModel())) ||
        !race || !part->validRaces || !part->validRaces->HasForm(race))
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
    if (BlankBeard(part)) return true;
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

static std::string PartName(const Head* part) {
    if (BlankBeard(part)) return "Без бороды";
    const auto* name = part->GetFullName();
    return name && *name ? name : part->GetFormEditorID();
}

static Color* BaseColor(RE::Actor* actor) {
    const auto* data = actor->GetActorBase()->headRelatedData;
    return data && data->hairColor ? data->hairColor : Face(actor)->defaultHairColor;
}

static int SkinValue(const RE::Color& color) {
    return (color.red << 16) | (color.green << 8) | color.blue;
}

static int SavedSkin(const json& saved) {
    const auto it = saved.find("skin");
    return it != saved.end() && it->is_number_integer() && *it >= 0 && *it <= 0xFFFFFF ? it->get<int>() : -1;
}

static RE::BSLightingShaderProperty* SkinShader(RE::BSGeometry* geometry) {
    return geometry ? netimmerse_cast<RE::BSLightingShaderProperty*>(geometry->GetGeometryRuntimeData().shaderProperty.get()) : nullptr;
}

static bool ApplySkin(RE::Actor* actor, int value) {
    using Feature = RE::BSShaderMaterial::Feature;
    using Flag = RE::BSShaderProperty::EShaderPropertyFlag8;
    auto* root = actor->Get3D(false);
    auto* face = actor->GetFaceNode();
    auto* base = actor->GetActorBase();
    auto* part = base ? base->GetHeadPartByType(PartType::kFace) : nullptr;
    auto* object = part && face ? face->GetObjectByName(part->formEditorID) : nullptr;
    auto* geometry = object ? object->AsGeometry() : nullptr;
    auto* shader = SkinShader(geometry);
    auto* manager = RE::BSFaceGenManager::GetSingleton();
    if (!root || !shader || !shader->material || !manager) return false;
    const auto feature = shader->material->GetFeature();
    if (feature != Feature::kFaceGen && feature != Feature::kFaceGenRGBTint) return false;
    if (value < 0) {
        manager->PrepareHeadPartForShaders(face, part, base);
        actor->UpdateSkinColor();
        return true;
    }
    const RE::NiColor rgb(((value >> 16) & 255) / 255.f, ((value >> 8) & 255) / 255.f, (value & 255) / 255.f);
    if (feature == Feature::kFaceGen) {
        // Печёный FaceGen не принимает RGB. Движок копирует временный материал в свой шейдер.
        using Material = RE::BSLightingShaderMaterialFacegenTint;
        auto* tint = RE::BSLightingShaderMaterialBase::CreateMaterial<Material>();
        if (!tint) return false;
        tint->CopyBaseMembers(static_cast<RE::BSLightingShaderMaterialBase*>(shader->material));
        tint->tintColor = rgb;
        shader->SetFlags(Flag::kFace, false);
        shader->SetFlags(Flag::kFaceGenRGBTint, true);
        shader->SetMaterial(tint, true);
        shader->SetupGeometry(geometry);
        shader->FinishSetupGeometry(geometry);
        tint->~Material();
        RE::MemoryManager::GetSingleton()->GetThreadScrapHeap()->Deallocate(tint);
    }
    if (!shader->material || shader->material->GetFeature() != Feature::kFaceGenRGBTint) return false;
    geometry->UpdateBodyTint(rgb);
    root->UpdateBodyTint(rgb);
    return true;
}

static json SkinPalette(RE::Actor* actor, int current) {
    auto options = json::array();
    const auto add = [&](int id, int rgb, const std::string& name) {
        if (std::ranges::any_of(options, [id](const json& option) { return option.at("id") == id; })) return;
        options.push_back({{"id", id}, {"name", name}, {"rgb", fmt::format("#{:06X}", rgb)}});
    };
    add(-1, SkinValue(actor->GetActorBase()->bodyTintColor), "Исходный тон");
    const auto append = [&](const Color* form) {
        if (!form) return;
        const auto rgb = SkinValue(form->color);
        const auto* name = form->GetFullName();
        add(rgb, rgb, name && *name ? name : fmt::format("#{:06X}", rgb));
    };
    using Tone = RE::TESRace::FaceRelatedData::TintAsset::TintLayer::SkinTone;
    if (auto* masks = Face(actor)->tintMasks) for (auto* asset : *masks) {
        if (!asset || asset->texture.skinTone != Tone::kSkinTone) continue;
        append(asset->texture.presetDefault);
        for (auto* form : asset->presets.colors) append(form);
    }
    if (current >= 0) add(current, current, fmt::format("#{:06X}", current));
    return options;
}

bool RestoreSkin(RE::Actor* actor, const json& saved) {
    const auto skin = SavedSkin(saved);
    return skin < 0 || ApplySkin(actor, skin);
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

static Head* SavedFacePart(const json& saved, const FacePart& field) {
    auto* part = Resolve<Head>(saved.value(field.key, json(nullptr)));
    return part && part->type == field.type && !part->IsExtraPart() ? part : nullptr;
}

static Head* BaseFacePart(RE::Actor* actor, const FacePart& field) {
    auto* base = actor->GetActorBase()->GetHeadPartByType(field.type);
    if (base || field.type != PartType::kFacialHair || actor->GetActorBase()->IsFemale()) return base;
    // Для чисто выбритого NPC без HDPT используем совместимую пустую запись.
    const auto& parts = RE::TESDataHandler::GetSingleton()->GetFormArray<Head>();
    const auto blank = std::ranges::find_if(parts, [&](auto* part) {
        return BlankBeard(part) && Compatible(actor, part, field.type);
    });
    return blank != parts.end() ? *blank : nullptr;
}

static Head* CurrentFacePart(RE::Actor* actor, const json& saved, const FacePart& field) {
    auto* base = BaseFacePart(actor, field);
    auto* part = SavedFacePart(saved, field);
    if (HasMesh(actor, part) || (BlankBeard(part) && !HasMesh(actor, base))) return part;
    return base;
}

static bool VisibleOrBlank(RE::Actor* actor, Head* part) {
    return BlankBeard(part) || HasMesh(actor, part);
}

static bool SwapFacePart(RE::Actor* actor, Head* previous, Head* next) {
    if (!actor->GetFaceNode() || !previous || !next || previous->type != next->type) return false;
    if (previous == next) return true;
    actor->ChangeHeadPart(previous, next);
    if (!BlankBeard(next)) return HasMesh(actor, next);
    return !HasMesh(actor, previous) && std::ranges::none_of(previous->extraParts, [&](Head* part) {
        return HasMesh(actor, part);
    });
}

bool OverridesHeadPart(const json& saved, Head* part) {
    const auto matches = [part](PartType type, Head* selected) {
        return part->type == type || (selected && std::ranges::find(selected->extraParts, part) != selected->extraParts.end());
    };
    if (saved.contains("hair") && matches(PartType::kHair, SavedHair(saved))) return true;
    return std::ranges::any_of(FaceParts, [&](const auto& field) {
        return saved.contains(field.key) && matches(field.type, SavedFacePart(saved, field));
    });
}

void RestoreBaseFaceParts(RE::Actor* actor, const json& saved) {
    for (const auto& field : FaceParts) {
        auto* part = SavedFacePart(saved, field);
        if (part && Compatible(actor, part, field.type) && SafeMeshes(part) &&
            actor->GetActorBase()->GetHeadPartByType(field.type) != part)
            actor->GetActorBase()->ChangeHeadPart(part);
    }
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
    result["height"] = Height(actor);
    result["skin"] = {{"value", SavedSkin(saved)}, {"options", SkinPalette(actor, SavedSkin(saved))}};
    auto* hair = BaseHair(actor);
    if (!managed) {
        auto* current = Resolve<Head>(saved.value("hair", json(nullptr)));
        if (HasMesh(actor, current)) hair = current;
    }
    auto* color = SavedColor(saved);
    if (!color) color = BaseColor(actor);
    result["hair"] = hair->GetFormID();
    result["hairName"] = PartName(hair);
    result["color"] = color ? color->GetFormID() : 0;
    result["hairAvailable"] = HasMesh(actor, hair);
    auto& hairs = result["hairs"] = json::array();
    const auto catalog = [&](PartType type, json& choices) {
        for (auto* part : RE::TESDataHandler::GetSingleton()->GetFormArray<Head>()) {
            if (!Compatible(actor, part, type)) continue;
            auto row = Choice(part, PartName(part));
            row["plugin"] = part->GetFile()->GetFilename();
            choices.push_back(std::move(row));
        }
        std::sort(choices.begin(), choices.end(), [](const json& left, const json& right) {
            return left.at("name").get_ref<const std::string&>() < right.at("name").get_ref<const std::string&>();
        });
    };
    catalog(PartType::kHair, hairs);
    auto& faceParts = result["faceParts"] = json::object();
    for (const auto& field : FaceParts) {
        auto* current = CurrentFacePart(actor, saved, field);
        if (!VisibleOrBlank(actor, current) || Address(current).is_null()) continue;
        auto options = json::array();
        catalog(field.type, options);
        const auto id = current->GetFormID();
        if (std::ranges::none_of(options, [id](const json& row) { return row.at("id") == id; }))
            options.insert(options.begin(), Choice(current, PartName(current)));
        if (options.size() > 1)
            faceParts[field.key] = {{"value", id}, {"options", std::move(options)}};
    }
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

bool TryOn(RE::Actor* actor, bool managed, const json& saved, const json& command, Draft& draft) {
    if (!Ready(actor, managed)) throw std::runtime_error("Внешность этого NPC сейчас недоступна");
    if (!draft.target) {
        auto* current = BaseHair(actor);
        if (!managed) if (auto* previous = SavedHair(saved); HasMesh(actor, previous)) current = previous;
        auto* color = SavedColor(saved);
        if (!color) color = BaseColor(actor);
        draft = {actor->GetHandle(), current, current, color, color};
        draft.skin = draft.originalSkin = SavedSkin(saved);
        for (std::size_t i = 0; i < FaceParts.size(); ++i) {
            auto* part = CurrentFacePart(actor, saved, FaceParts[i]);
            draft.faceParts[i] = {part, part};
        }
    }
    auto* hair = RE::TESForm::LookupByID<Head>(command.at("hair").get<RE::FormID>());
    const auto colorId = command.at("color").get<RE::FormID>();
    const auto skin = command.value("skin", draft.skin);
    if (skin != draft.skin && skin != draft.originalSkin) {
        const auto palette = SkinPalette(actor, draft.originalSkin);
        if (std::ranges::none_of(palette, [skin](const json& option) { return option.at("id") == skin; }))
            throw std::runtime_error("Оттенок кожи недоступен для этого NPC");
    }
    auto* color = colorId ? RE::TESForm::LookupByID<Color>(colorId) : nullptr;
    if (hair != draft.originalHair && (!Compatible(actor, hair) || !SafeMeshes(hair)))
        throw std::runtime_error("Причёска несовместима, отсутствует модель или требуется Faster HDT-SMP");
    if ((colorId && !color) || (color != draft.originalColor && !CompatibleColor(actor, color)))
        throw std::runtime_error("Цвет недоступен для этого NPC");
    if (color != draft.originalColor && !draft.originalColor)
        throw std::runtime_error("Исходный цвет волос недоступен");
    if (!HasMesh(actor, draft.hair) && HasMesh(actor, draft.originalHair)) draft.hair = draft.originalHair;
    const bool changedHair = hair != draft.hair;
    if ((changedHair || color != draft.color) && !HasMesh(actor, draft.hair))
        throw std::runtime_error("Снимите головной убор и дождитесь загрузки волос");
    for (auto& part : draft.faceParts) {
        if (part.current != part.original && !HasMesh(actor, part.current) && HasMesh(actor, part.original))
            part.current = part.original;
    }
    auto parts = draft.faceParts;
    const auto requested = command.find("faceParts");
    if (requested != command.end() && !requested->is_object())
        throw std::runtime_error("Некорректный набор деталей лица");
    for (std::size_t i = 0; requested != command.end() && i < FaceParts.size(); ++i) {
        const auto& field = FaceParts[i];
        if (!requested->contains(field.key)) continue;
        auto* next = RE::TESForm::LookupByID<Head>(requested->at(field.key).get<RE::FormID>());
        auto& part = parts[i];
        if (!next || !part.original ||
            (next != part.original && (!Compatible(actor, next, field.type) || !SafeMeshes(next))))
            throw std::runtime_error("Деталь лица несовместима или её модель недоступна");
        if (next != part.current && !VisibleOrBlank(actor, part.current))
            throw std::runtime_error("Исходная деталь лица не загружена");
        part.current = next;
    }
    if (changedHair) {
        auto* previous = draft.hair;
        draft.hair = hair;
        actor->ChangeHeadPart(previous, hair);
        if (!HasMesh(actor, hair)) {
            Cancel(draft);
            throw std::runtime_error("Движок не подтвердил замену волос");
        }
    }
    bool changedFace = false;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        auto& part = draft.faceParts[i];
        auto* previous = part.current;
        if (previous == parts[i].current) continue;
        part.current = parts[i].current;
        if (!SwapFacePart(actor, previous, part.current)) {
            Cancel(draft);
            throw std::runtime_error("Движок не подтвердил замену детали лица");
        }
        changedFace = true;
    }
    draft.color = color;
    Tint(actor, color);
    if (skin != draft.skin) {
        if (!ApplySkin(actor, skin)) {
            Cancel(draft);
            throw std::runtime_error("Материал кожи этого NPC не поддерживается");
        }
        draft.skin = skin;
    }
    return changedHair || changedFace;
}

json Commit(RE::Actor* actor, bool managed, const json& saved, Draft& draft) {
    auto result = saved;
    if (draft.skin != draft.originalSkin) {
        if (draft.skin < 0) result.erase("skin");
        else result["skin"] = draft.skin;
    }
    if (draft.hair != draft.originalHair) {
        if (managed) actor->GetActorBase()->ChangeHeadPart(draft.hair);
        result["hair"] = !managed && draft.hair == BaseHair(actor) ? json(nullptr) : Address(draft.hair);
    }
    if (draft.color != draft.originalColor) {
        if (managed) actor->GetActorBase()->SetHairColor(draft.color);
        result["color"] = Address(draft.color);
    }
    for (std::size_t i = 0; i < FaceParts.size(); ++i) {
        const auto& part = draft.faceParts[i];
        if (part.current == part.original) continue;
        if (managed) actor->GetActorBase()->ChangeHeadPart(part.current);
        const auto& field = FaceParts[i];
        if (!managed && part.current == BaseFacePart(actor, field)) result.erase(field.key);
        else result[field.key] = Address(part.current);
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
        for (const auto& part : draft.faceParts) {
            if (part.current == part.original || HasMesh(actor, part.original)) continue;
            SwapFacePart(actor, part.current, part.original);
        }
        if (draft.skin != draft.originalSkin) ApplySkin(actor, draft.originalSkin);
        Tint(actor, draft.originalColor);
    }
    draft = {};
}

bool RestoreExternal(RE::Actor* actor, const json& saved) {
    if (!Ready(actor, false)) return false;
    auto* original = BaseHair(actor);
    auto* hair = SavedHair(saved);
    if (!CanRestoreHair(actor, hair)) hair = nullptr;
    bool complete = VisibleOrBlank(actor, hair ? hair : original);
    if (hair && !HasMesh(actor, hair)) {
        complete = HasMesh(actor, original);
        if (complete && !SwapFacePart(actor, original, hair))
            logger::warn("После загрузки NPC {:08X} движок не подтвердил замену волос", actor->GetFormID());
    }
    for (const auto& field : FaceParts) {
        auto* part = SavedFacePart(saved, field);
        if (!part || !Compatible(actor, part, field.type) || !SafeMeshes(part) || HasMesh(actor, part)) continue;
        auto* base = BaseFacePart(actor, field);
        if (BlankBeard(part) && !HasMesh(actor, base)) continue;
        if (!VisibleOrBlank(actor, base)) complete = false;
        else if (!SwapFacePart(actor, base, part))
            logger::warn("После загрузки NPC {:08X} движок не подтвердил замену {}", actor->GetFormID(), field.key);
    }
    if (auto* color = SavedColor(saved); CompatibleColor(actor, color)) Tint(actor, color);
    RestoreSkin(actor, saved);
    return complete;
}
}
