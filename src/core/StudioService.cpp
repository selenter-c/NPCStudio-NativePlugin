#include "preview/ActorPreview.h"
#include "core/RaceService.h"
#include "core/StudioService.h"
#include "core/StateCodec.h"
#include "core/EquipmentService.h"
#include "core/AppearanceService.h"
#include "core/NameCompatibility.h"
#include "ui/MagelightBridge.h"
#include <fstream>
#include <unordered_set>

namespace NPCStudio {
using json = nlohmann::json;
static constexpr std::uint32_t StateTag = 0x5354534E;
static constexpr std::uint32_t StateVersion = 2;

StudioService& StudioService::Get() {
    static StudioService instance;
    return instance;
}

void StudioService::Initialize() {
    try {
        std::ifstream file("Data/SKSE/Plugins/NPCStudio/slots.json");
        auto manifest = json::parse(file);
        if (manifest.at("schemaVersion") != 3 || manifest.at("plugin") != "NPCStudio.esp" || manifest.at("slots").size() != SlotCount)
            throw std::runtime_error("Неверный манифест пула NPCStudio");
        auto* data = RE::TESDataHandler::GetSingleton();
        holdingAnchor = data->LookupForm<RE::TESObjectREFR>(0x801, "NPCStudio.esp");
        if (!holdingAnchor) throw std::runtime_error("Обновите NPCStudio.esp: отсутствует точка возврата в резерв");
        std::unordered_set<RE::FormID> bases, actors;
        std::vector<Slot> resolved;
        for (const auto& entry : manifest.at("slots")) {
            Slot slot;
            slot.id = entry.at("id").get<unsigned>();
            slot.female = entry.at("female").get<bool>();
            slot.base = data->LookupForm<RE::TESNPC>(entry.at("base").get<unsigned>(), "NPCStudio.esp");
            slot.actor = data->LookupForm<RE::Actor>(entry.at("reference").get<unsigned>(), "NPCStudio.esp");
            if (slot.id != resolved.size() || !slot.base || !slot.actor || slot.actor->GetActorBase() != slot.base ||
                slot.base->IsFemale() != slot.female || slot.female != FemaleSlot(slot.id) || !bases.insert(slot.base->GetFormID()).second ||
                !actors.insert(slot.actor->GetFormID()).second)
                throw std::runtime_error("ESP и манифест слотов не совпадают");
            slot.defaultName = slot.base->GetName();
            slot.defaultRace = slot.base->GetRace();
            if (slot.base->headRelatedData) slot.defaultHairColor = slot.base->headRelatedData->hairColor;
            Names::Protect(slot.base);
            resolved.push_back(std::move(slot));
        }
        slots = std::move(resolved);
        ready = true;
        worker = std::jthread([this](std::stop_token stop) {
            unsigned ticks = 0;
            while (!stop.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                const bool checkExternal = ++ticks % 4 == 0 && externalWork.load();
                if ((!workPending.load() && !checkExternal) || taskQueued.exchange(true)) continue;
                const auto epoch = worldEpoch.load();
                SKSE::GetTaskInterface()->AddTask([this, epoch] {
                    if (epoch == worldEpoch.load()) Pump();
                    taskQueued = false;
                });
            }
        });
        logger::info("Загружен пул: {} постоянных слотов", slots.size());
    } catch (const std::exception& e) {
        ready = false;
        error = e.what();
        logger::error("Пул NPCStudio недоступен: {}", error);
    }
}

StudioService::Slot& StudioService::Find(unsigned id) {
    if (id >= slots.size()) throw std::runtime_error("Неизвестный слот");
    return slots[id];
}

json StudioService::Snapshot() const {
    json list = json::array(), available = json::array();
    unsigned females = 0, males = 0;
    for (const auto& slot : slots) {
        const auto key = std::to_string(slot.id);
        if (!assignments.contains(key)) {
            available.push_back({{"id", slot.id}, {"female", slot.female}, {"number", SlotNumber(slot.id)},
                {"history", usedSlots.contains(slot.id) ? "used" : "fresh"}});
            continue;
        }
        const auto& saved = assignments.at(key);
        slot.female ? ++females : ++males;
        list.push_back({{"id", slot.id}, {"female", slot.female}, {"number", SlotNumber(slot.id)}, {"warning", saved.value("warning", std::string())}, {"audit", saved.value("audit", json::object())}, {"restoring", restoring.contains(slot.id)}, {"name", saved.at("name")}, {"identity", saved.at("identity").get<std::string>()},
            {"location", slot.actor->GetParentCell() ? std::string(slot.actor->GetParentCell()->GetName()) : std::string()}, {"parked", slot.actor->IsDisabled()}, {"dead", slot.actor->IsDead()},
            {"reference", fmt::format("{:08X}", slot.actor->GetFormID())},
            {"raceId", slot.actor->GetRace() ? slot.actor->GetRace()->GetFormEditorID() : ""}, {"race", slot.actor->GetRace() ? slot.actor->GetRace()->GetName() : ""}});
    }
    return {{"version", "0.8.5"}, {"races", ready ? Races::List() : json::array()}, {"ready", ready && stateValid}, {"worldLoaded", loaded}, {"error", error},
        {"femaleUsed", females}, {"maleUsed", males}, {"slots", list}, {"availableSlots", available}, {"capacity", 200}, {"library", Equipment::Library()}, {"settings", UI::Settings()},
        {"capabilities", {{"preview", true}, {"equipment", true}, {"appearance", true}, {"biography", false}}}};
}

void StudioService::PickAppearanceTarget() {
    appearanceTarget.reset();
    if (auto* crosshair = RE::CrosshairPickData::GetSingleton()) appearanceTarget = crosshair->GetActiveTarget();
}
void StudioService::CancelAppearance() {
    lookFrame.reset();
    lookClassicPose = false;
    if (auto target = hairDraft.target.get()) {
        if (auto it = externalLooks.find(target->GetFormID()); it != externalLooks.end()) it->second.appliedNode = nullptr;
    }
    Appearance::Cancel(hairDraft);
}

json StudioService::QueueLookFrame(RE::Actor* actor, RE::BGSHeadPart* hair, const std::string& identity, const json& options) {
    lookFrame = LookFrame{actor->GetHandle(), hair, identity, options, std::chrono::steady_clock::now() + std::chrono::milliseconds(35)};
    workPending = true;
    UI::SetWorldWork(true);
    return {{"partial", true}, {"preview", {{"identity", identity}, {"kind", "actor"}, {"pending", true}}}};
}

json StudioService::RenderLookFrame(RE::Actor* actor, const std::string& identity, json options) {
    options["portraitOnly"] = false;
    options["classicPose"] = lookClassicPose;
    return Preview::Frame(actor, identity, options);
}

json StudioService::Execute(const json& command) {
    const auto action = command.at("action").get<std::string>();
    if (action == "state") return Snapshot();
    if (action == "openState") {
        auto state = Snapshot();
        auto target = appearanceTarget.get();
        auto* actor = target ? target->As<RE::Actor>() : nullptr;
        if (!actor || actor->IsPlayerRef() || actor->IsDead() || !actor->GetActorBase()) return state;
        for (const auto& slot : slots) {
            if (slot.actor != actor) continue;
            if (assignments.contains(std::to_string(slot.id))) state["appearanceTarget"] = {{"id", slot.id}};
            return state;
        }
        state["appearanceTarget"] = {{"reference", actor->GetFormID()}};
        return state;
    }
    if (action == "deletePreset" || action == "deleteCategory") { Equipment::DeleteCustom(action, command.at("key").get<std::string>()); return Snapshot(); }
    if (action == "portraitCache") {
        const auto& identities = command.at("identities");
        if (!identities.is_array() || identities.size() > 24) throw std::runtime_error("Invalid portrait batch");
        json images = json::object();
        for (const auto& identity : identities) {
            const auto key = identity.get<std::string>();
            for (const auto& saved : assignments.items()) {
                if (saved.value().at("identity") != key) continue;
                auto image = Preview::ReadPortrait(key);
                if (!image.empty()) images[key] = std::move(image);
                break;
            }
        }
        return {{"partial", true}, {"portraits", images}};
    }
    if (action == "previewStop") { Preview::Clear(); CancelAppearance(); return {{"partial", true}}; }
    if (action == "hairThumbnailCancel") {
        Preview::CancelHairThumbnail(command.at("token").get<unsigned>());
        return {{"partial", true}};
    }
    if (action == "saveSettings") { UI::SaveSettings(command.at("settings")); return Snapshot(); }
    if (action == "addCategory") { Equipment::AddCategory(command.at("name").get<std::string>()); return Snapshot(); }
    if (!ready || !stateValid) throw std::runtime_error(error.empty() ? "Пул недоступен" : error);
    if (!loaded || !RE::PlayerCharacter::GetSingleton()->GetParentCell())
        throw std::runtime_error("Сначала загрузите сохранение или начните новую игру");
    if (action == "editPreset") { Equipment::SaveEditedPreset(command); return Snapshot(); }
    if (action == "presetCatalog") return {{"partial",true},{"catalog",Equipment::Describe(nullptr, command).at("catalog")}};
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (action == "appearance" || action == "applyAppearance" || action == "tryAppearance" || action == "setHeight" ||
        action == "appearanceFrame" || action == "hairThumbnail" || action == "hairThumbnailCache") {
        const bool managed = command.contains("id");
        RE::Actor* actor = nullptr;
        json* assignment = nullptr;
        RE::NiPointer<RE::TESObjectREFR> target;
        json saved = json::object();
        if (managed) {
            auto& slot = Find(command.at("id").get<unsigned>());
            const auto key = std::to_string(slot.id);
            if (!assignments.contains(key) || assignments.at(key).at("identity") != command.at("identity"))
                throw std::runtime_error("Персонаж в этом слоте уже изменился. Обновите список");
            if (restoring.contains(slot.id)) throw std::runtime_error("Дождитесь восстановления снаряжения");
            actor = slot.actor;
            assignment = &assignments.at(key);
            saved = assignment->value("appearance", json::object());
        } else {
            target = appearanceTarget.get();
            actor = target ? target->As<RE::Actor>() : nullptr;
            if (!actor || !actor->GetActorBase() || actor->GetFormID() != command.at("reference").get<RE::FormID>())
                throw std::runtime_error("NPC под прицелом больше недоступен");
            if (auto it = externalLooks.find(actor->GetFormID()); it != externalLooks.end() &&
                it->second.base == actor->GetActorBase()->GetFormID()) saved = it->second.look;
        }
        if (action == "setHeight") {
            Preview::Clear();
            Appearance::SetHeight(actor, managed, command.at("percent"));
            settlingTicks = 3;
            workPending = true;
            UI::SetWorldWork(true);
            return {{"partial", true}, {"height", Appearance::Height(actor)}};
        }
        if (action == "hairThumbnailCache") {
            const auto& ids = command.at("hairs");
            if (!ids.is_array() || ids.size() > 8) throw std::runtime_error("Invalid hair thumbnail batch");
            auto images = json::object();
            for (const auto& value : ids) {
                try {
                    const auto id = value.get<RE::FormID>();
                    const auto [part, fingerprint] = Appearance::ThumbnailPart(actor, id);
                    if (auto image = Preview::ReadHairThumbnail(fingerprint); !image.empty())
                        images[std::to_string(id)] = std::move(image);
                } catch (const std::exception&) { /* Недоступная модель не мешает соседним карточкам. */ }
            }
            return {{"partial", true}, {"hairThumbnails", std::move(images)}};
        }
        if (action == "hairThumbnail") {
            lookFrame.reset();
            const auto [part, fingerprint] = Appearance::ThumbnailPart(actor, command.at("hair").get<RE::FormID>());
            return {{"partial", true}, {"hairThumbnail", Preview::HairThumbnail(part, command.at("token").get<unsigned>(), fingerprint)}};
        }
        const auto previewIdentity = fmt::format("look-{:08X}", actor->GetFormID());
        if (action == "appearanceFrame") {
            if (lookFrame && lookFrame->identity == previewIdentity) {
                lookFrame->options = command;
                return {{"partial", true}, {"preview", {{"identity", previewIdentity}, {"kind", "actor"}, {"pending", true}}}};
            }
            if (command.value("resume", false)) {
                Preview::Clear();
                lookClassicPose = false;
                auto* hair = hairDraft.target.get().get() == actor ? hairDraft.hair : Appearance::VisualHair(actor, saved);
                if (Appearance::HairAwaitingBones(actor, hair)) return QueueLookFrame(actor, hair, previewIdentity, command);
            }
            return {{"partial", true}, {"preview", RenderLookFrame(actor, previewIdentity, command)}};
        }
        if (action == "applyAppearance" || action == "tryAppearance") {
            if (action == "applyAppearance" && !managed && externalLooks.size() >= 128 && !externalLooks.contains(actor->GetFormID()))
                throw std::runtime_error("Достигнут лимит внешних NPC");
            Preview::Clear();
            if (hairDraft.target && hairDraft.target.get().get() != actor) CancelAppearance();
            const bool changedHair = Appearance::TryOn(actor, managed, saved, command, hairDraft);
            if (action == "tryAppearance") {
                if (changedHair) return QueueLookFrame(actor, hairDraft.hair, previewIdentity, command);
                if (lookFrame && lookFrame->identity == previewIdentity) {
                    lookFrame->options = command;
                    return {{"partial", true}, {"preview", {{"identity", previewIdentity}, {"kind", "actor"}, {"pending", true}}}};
                }
                return {{"partial", true}, {"preview", RenderLookFrame(actor, previewIdentity, command)}};
            }
            saved = Appearance::Commit(actor, managed, saved, hairDraft);
            if (managed) (*assignment)["appearance"] = saved;
            else if (!saved.empty()) {
                externalLooks.insert_or_assign(actor->GetFormID(), ExternalLook{
                    actor->GetActorBase()->GetFormID(), saved, actor->GetFaceNode()});
                externalWork = true;
            }
        } else if (hairDraft.target) {
            Preview::Clear();
            CancelAppearance();
        }
        auto description = Appearance::Describe(actor, managed, saved);
        description["previewIdentity"] = previewIdentity;
        return {{"partial", true}, {"appearance", std::move(description)}};
    }
    if (action == "itemPreview") {
        const auto& item = command.at("item");
        auto* form = RE::TESDataHandler::GetSingleton()->LookupForm(item.at("form").get<unsigned>(), item.at("plugin").get<std::string>());
        auto* object = form ? form->As<RE::TESBoundObject>() : nullptr;
        return {{"partial", true}, {"itemPreview", Preview::ItemFrame(object, command)}};
    }
    if (action == "randomName") {
        bool female = command.value("female", true);
        RE::TESNPC* base = nullptr;
        if (command.contains("id")) {
            auto& slot = Find(command.at("id").get<unsigned>());
            if (!assignments.contains(std::to_string(slot.id))) throw std::runtime_error("Слот не занят");
            female = slot.female; base = slot.base;
        } else {
            for (auto& slot : slots) if (slot.female == female) { base = slot.base; break; }
        }
        const auto raceId = command.value("race", base && base->GetRace() ? std::string(base->GetRace()->GetFormEditorID()) : "NordRace");
        auto name = Names::Random(female, Races::Resolve(raceId)->GetFormEditorID());
        auto state = Snapshot(); state["generatedName"] = name; return state;
    }
    if (action == "create") {
        const bool female = command.at("female").get<bool>();
        auto name = command.at("name").get<std::string>();
        if (!ValidName(name))
            throw std::runtime_error("Имя должно содержать от 1 до 160 байт без переноса строки");
        auto it = slots.end();
        if (command.contains("slot") && !command.at("slot").is_null()) {
            const auto id = command.at("slot").get<unsigned>();
            if (id >= slots.size()) throw std::runtime_error("Неизвестный слот");
            it = slots.begin() + id;
            if (it->female != female || assignments.contains(std::to_string(id))) throw std::runtime_error("Слот занят или не соответствует выбранному полу");
        } else {
            int priority = 3;
            PoolState history{assignments, usedSlots};
            for (auto candidate = slots.begin(); candidate != slots.end(); ++candidate) {
                if (candidate->female != female || assignments.contains(std::to_string(candidate->id))) continue;
                const auto rank = SlotHistoryPriority(candidate->id, history);
                if (rank < priority) { priority = rank; it = candidate; }
            }
        }
        if (it == slots.end()) throw std::runtime_error("Свободных слотов выбранного пола нет");
        const auto preset = command.value("preset", std::string());
        if (!preset.empty()) Equipment::ValidatePreset(preset);
        auto& slot = *it;
        if (slot.actor->IsDead()) throw std::runtime_error("Резервный актёр погиб: слот требует диагностики");
        auto* race = Races::Resolve(command.value("race", female ? std::string("NordRace") : std::string("WoodElfRace")));
        Names::Apply(slot.base, slot.actor, name);
        slot.actor->MoveTo(player);
        slot.actor->Enable(false);
        assignments[std::to_string(slot.id)] = {{"name", name}, {"identity", fmt::format("slot-{}-{}-{}", slot.id, std::chrono::system_clock::now().time_since_epoch().count(), GetCurrentProcessId())}};
        assignments[std::to_string(slot.id)]["race"] = race->GetFormEditorID();
        usedSlots.insert(slot.id);
        QueueRestore(slot, preset);
        logger::info("Создан NPC '{}' в слоте {}, reference {:08X}", name, slot.id, slot.actor->GetFormID());
    } else {
        auto& slot = Find(command.at("id").get<unsigned>());
        const auto key = std::to_string(slot.id);
        if (!assignments.contains(key)) throw std::runtime_error("Слот ещё не занят");
        const auto identity = assignments.at(key).at("identity").get<std::string>();
        if (command.contains("identity") && command.at("identity") != identity)
            throw std::runtime_error("Персонаж в этом слоте уже изменился. Обновите список");
        if (action == "savePortrait") {
            Preview::SavePortrait(identity, command.at("image").get<std::string>());
            return {{"partial", true}};
        } else if (action == "preview" || action == "portrait") {
            if (restoring.contains(slot.id)) throw std::runtime_error("Дождитесь восстановления снаряжения");
            auto options = command; options["portraitOnly"] = action == "portrait";
            auto preview = Preview::Frame(slot.actor, identity, options);
            return {{"partial", true}, {action == "portrait" ? "portrait" : "preview", std::move(preview)}};
        } else if (action == "delete") {
            if (restoring.contains(slot.id)) throw std::runtime_error("Дождитесь восстановления снаряжения");
            if (!command.value("confirmed", false) || command.value("identity", std::string()) != identity)
                throw std::runtime_error("Подтвердите удаление выбранного персонажа");
            Delete(slot);
        } else if (action == "summon") {
            if (slot.actor->IsDead()) throw std::runtime_error("NPC погиб. Призыв не воскрешает персонажа");
            if (!slot.actor->IsDisabled() && slot.actor->Is3DLoaded() && !restoring.contains(slot.id)) assignments[key]["loadout"] = Equipment::Capture(slot.actor);
            slot.actor->MoveTo(player);
            slot.actor->Enable(false);
            QueueRestore(slot);
        } else if (action == "park") {
            // Слот остаётся занятым: его личность и сохранённые данные не переиспользуются.
            if (slot.actor->IsDisabled()) return Snapshot();
            if (restoring.contains(slot.id)) throw std::runtime_error("Дождитесь восстановления снаряжения");
            assignments[key]["loadout"] = Equipment::Capture(slot.actor);
            Preview::Clear();
            slot.actor->Disable();
        } else if (action == "teleport") {
            if (slot.actor->IsDisabled()) throw std::runtime_error("NPC убран из мира. Сначала призовите его");
            player->MoveTo(slot.actor);
        } else if (action == "setRace") {
            if (slot.actor->IsDisabled() || slot.actor->IsDead() || restoring.contains(slot.id)) throw std::runtime_error("NPC is not ready for a race change");
            auto* race = Races::Resolve(command.at("race").get<std::string>());
            assignments[key]["loadout"] = Equipment::Capture(slot.actor);
            Preview::Clear();
            assignments[key]["race"] = race->GetFormEditorID();
            assignments[key].erase("headParts");
            assignments[key].erase("faceMorphs");
            assignments[key].erase("facePresets");
            assignments[key].erase("appearance");
            QueueRestore(slot);
        } else if (action == "rename") {
            auto name = command.at("name").get<std::string>();
            if (!ValidName(name))
                throw std::runtime_error("Некорректное имя");
            Names::Apply(slot.base, slot.actor, name);
            assignments[key]["name"] = name;
        } else if (action == "equipment" || action == "giveItem" || action == "equipItem" || action == "unequipItem" || action == "removeItem" ||
            action == "applyEquipmentPreset") {
            if (restoring.contains(slot.id)) throw std::runtime_error("Снаряжение ещё восстанавливается");
            if (action != "equipment") {
                settlingTicks = 3;
                workPending = true;
                UI::SetWorldWork(true);
                Equipment::Execute(slot.actor, command);
                assignments[key]["loadout"] = Equipment::Capture(slot.actor);
                assignments[key].erase("audit");
                assignments[key].erase("warning");
            }
            auto state = Snapshot();
            state["equipment"] = Equipment::Describe(slot.actor, command);
            state["equipmentOwnerId"] = slot.id;
            return state;
        } else throw std::runtime_error("Неизвестная команда");
    }
    return Snapshot();
}

void StudioService::Delete(Slot& slot) {
    if (!holdingAnchor) throw std::runtime_error("Резервная точка недоступна");
    Preview::Clear();
    restoring.erase(slot.id);
    usedSlots.insert(slot.id);
    slot.actor->StopCombat();
    // Предметы удаляются, а не выбрасываются в мир и не передаются игроку.
    for (auto& [object, entry] : slot.actor->GetInventory()) {
        if (object && entry.first > 0)
            slot.actor->RemoveItem(object, entry.first, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
    }
    for (const auto& [object, entry] : slot.actor->GetInventory()) {
        if (object && entry.first > 0) throw std::runtime_error("Инвентарь очищен не полностью; слот пока не освобождён");
    }
    if (slot.actor->IsDead()) slot.actor->Resurrect(false, false);
    for (const auto value : {RE::ActorValue::kHealth, RE::ActorValue::kStamina, RE::ActorValue::kMagicka}) {
        const auto damage = slot.actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, value);
        slot.actor->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, value, -damage);
    }
    slot.actor->Disable();
    if (slot.actor->GetReferenceRuntimeData().refScale != 100) {
        slot.actor->SetScale(1.f);
        slot.actor->AddChange(RE::TESObjectREFR::ChangeFlags::kScale);
    }
    slot.actor->MoveTo(holdingAnchor);
    Names::Apply(slot.base, slot.actor, slot.defaultName);
    const auto oldName = assignments.at(std::to_string(slot.id)).at("name").get<std::string>();
    assignments.erase(std::to_string(slot.id));
    logger::info("Удалён NPC '{}' из слота {}; reference {:08X} возвращена в резерв", oldName, slot.id, slot.actor->GetFormID());
}

void StudioService::QueueRestore(Slot& slot, const std::string& preset) {
    const auto key = std::to_string(slot.id);
    if (!preset.empty()) assignments[key]["pendingPreset"] = preset;
    restoring[slot.id] = {assignments.at(key).at("identity").get<std::string>(), assignments.at(key).value("pendingPreset", std::string())};
    assignments[key]["warning"] = "Восстановление снаряжения…";
    workPending = true;
    UI::SetWorldWork(true);
}
void StudioService::Pump() {
    if (!loaded || !stateValid) { lookFrame.reset(); restoring.clear(); workPending = false; externalWork = false; UI::SetWorldWork(false); return; }
    if (lookFrame && std::chrono::steady_clock::now() >= lookFrame->earliest) {
        auto target = lookFrame->target.get();
        auto* actor = target ? target->As<RE::Actor>() : nullptr;
        const bool unbound = actor && Appearance::HairAwaitingBones(actor, lookFrame->hair);
        if (!unbound || ++lookFrame->attempts >= 2) {
            auto frame = std::move(*lookFrame);
            lookFrame.reset();
            lookClassicPose = unbound;
            if (unbound) logger::info("Превью {}: волосы ещё без привязки, используем исходную позу", frame.identity);
            try { RenderLookFrame(actor, frame.identity, std::move(frame.options)); }
            catch (const std::exception& e) { Preview::PublishFrameError(frame.identity, e.what()); }
        }
    }
    const auto draftTarget = hairDraft.target.get();
    for (auto& [id, edit] : externalLooks) {
        if (draftTarget && draftTarget->GetFormID() == id) continue;
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
        auto* base = actor ? actor->GetActorBase() : nullptr;
        auto* node = base && base->GetFormID() == edit.base && actor->Is3DLoaded() ? actor->GetFaceNode() : nullptr;
        if (!node) edit.appliedNode = nullptr;
        else if (node != edit.appliedNode && Appearance::RestoreExternal(actor, edit.look)) edit.appliedNode = node;
    }
    if (settlingTicks) --settlingTicks;
    for (auto it = restoring.begin(); it != restoring.end();) {
        auto& slot = Find(it->first);
        auto& job = it->second;
        const auto key = std::to_string(slot.id);
        if (!assignments.contains(key) || assignments.at(key).at("identity").get<std::string>() != job.identity || slot.actor->IsDisabled()) { it = restoring.erase(it); continue; }
        try {
            if (++job.attempts > 40) throw std::runtime_error("Модель не загрузилась за 6 секунд; призовите NPC повторно");
            if (!slot.actor->Is3DLoaded()) { ++it; continue; }
            auto* targetRace = Races::Resolve(assignments.at(key).at("race").get<std::string>());
            if (slot.actor->GetRace() != targetRace) {
                if (!job.raceRequested) {
                    Races::Request(slot.actor, targetRace);
                    job.raceRequested = true;
                    job.stage = 1;
                }
                ++it; continue;
            }
            if (job.stage > 0 && job.stage++ < 4) { ++it; continue; }
            if (!job.appearanceRestored) {
                job.appearanceRestored = true;
                // RNAM базы не обязан сохраняться вместе с runtime-расой Actor.
                slot.base->race = targetRace;
                // Как в RaceMenu: сначала базовый набор расы, затем сохранённые части.
                auto* face = targetRace->faceRelatedData[slot.female ? 1 : 0];
                if (!face || !face->headParts || face->headParts->empty() || face->headParts->size() > 127)
                    throw std::runtime_error("Race has no valid default head parts");
                auto* parts = RE::calloc<RE::BGSHeadPart*>(face->headParts->size());
                if (!parts) throw std::bad_alloc();
                std::copy(face->headParts->begin(), face->headParts->end(), parts);
                RE::free(slot.base->headParts);
                slot.base->headParts = parts;
                slot.base->numHeadParts = static_cast<std::int8_t>(face->headParts->size());
                slot.base->originalRace = nullptr;
                slot.base->faceNPC = nullptr;
                logger::info("Восстановление внешности слота {}: actor={}, base={}, headParts={}", slot.id, targetRace->GetFormEditorID(), slot.base->race->GetFormEditorID(), slot.base->numHeadParts);
                const auto look = assignments[key].value("appearance", json::object());
                auto* savedHair = Appearance::SavedHair(look);
                if (assignments[key].contains("headParts")) {
                    for (const auto& part : assignments[key]["headParts"]) {
                        auto* head = RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSHeadPart>(part.at("form").get<unsigned>(), part.at("plugin").get<std::string>());
                        if (!head) {
                            logger::warn("Не найдена сохранённая часть головы слота {}: {}", slot.id, part.dump());
                            continue;
                        }
                        if (look.contains("hair") && (head->type == RE::BGSHeadPart::HeadPartType::kHair ||
                            (savedHair && std::ranges::find(savedHair->extraParts, head) != savedHair->extraParts.end()))) continue;
                        slot.base->ChangeHeadPart(head);
                    }
                }
                if (savedHair && savedHair != slot.base->GetHeadPartByType(RE::BGSHeadPart::HeadPartType::kHair) &&
                    Appearance::CanRestoreHair(slot.actor, savedHair)) slot.base->ChangeHeadPart(savedHair);
                auto* color = Appearance::SavedColor(look);
                if (!color) color = targetRace == slot.defaultRace && slot.defaultHairColor ? slot.defaultHairColor : face->defaultHairColor;
                slot.base->SetHairColor(color);
                if (assignments[key].contains("faceMorphs")) {
                    const auto& morphs = assignments[key]["faceMorphs"];
                    const auto& presets = assignments[key].at("facePresets");
                    if (morphs.size() == 19 && presets.size() == 4) {
                        if (!slot.base->faceData) slot.base->faceData = RE::calloc<RE::TESNPC::FaceData>(1);
                        if (!slot.base->faceData) throw std::bad_alloc();
                        for (unsigned i = 0; i < 19; ++i) slot.base->faceData->morphs[i] = morphs[i].get<float>();
                        for (unsigned i = 0; i < 4; ++i) slot.base->faceData->parts[i] = presets[i].get<int>();
                    }
                }
                // Update3DModel обновляет экипировку; DoReset3D пересобирает также FaceGen.
                slot.actor->DoReset3D(false);
                job.stage = 1;
                ++it; continue;
            }
            Races::SynchronizeSkin(slot.actor);
            slot.actor->UpdateHairColor();
            if (!job.preset.empty()) {
                Equipment::Execute(slot.actor, {{"action", "applyEquipmentPreset"}, {"preset", job.preset}});
                assignments[key]["loadout"] = Equipment::Capture(slot.actor);
                assignments[key].erase("pendingPreset");
                job.preset.clear(); job.stage = 1; ++it; continue;
            }
            if (assignments.at(key).contains("loadout")) {
                const auto audit = Equipment::Restore(slot.actor, assignments.at(key).at("loadout"));
                assignments[key]["audit"] = audit;
                if (job.stage == 0) { job.stage = 1; ++it; continue; }
                assignments[key]["warning"] = audit.at("missing").empty() && audit.at("notEquipped").empty() ?
                    fmt::format("Снаряжение восстановлено. Проверено вещей: {}; динамических пропущено: {}", audit.at("checked").get<unsigned>(), audit.at("skipped").get<unsigned>()) :
                    fmt::format("Недостача: {}; не надето: {}. Проверьте снаряжение", audit.at("missing").size(), audit.at("notEquipped").size());
                logger::info("Проверка снаряжения слот {}: {}", slot.id, audit.dump());
            } else assignments[key]["warning"] = "";
            it = restoring.erase(it);
        } catch (const std::exception& e) { assignments[key]["warning"] = e.what(); logger::warn("Восстановление слот {}: {}", slot.id, e.what()); it = restoring.erase(it); }
    }
    workPending = !restoring.empty() || settlingTicks > 0 || lookFrame.has_value();
    UI::SetWorldWork(workPending.load());
}
void StudioService::BeforeLoad() { Preview::Clear(); ++worldEpoch; lookFrame.reset(); restoring.clear(); settlingTicks = 0; workPending = false; externalWork = false; appearanceTarget.reset(); loaded = false; UI::SetWorldWork(false); }

void StudioService::RestoreNames() {
    if (!ready || !stateValid) return;
    for (auto& slot : slots) {
        auto key = std::to_string(slot.id);
        const auto name = assignments.contains(key) ? assignments.at(key).at("name").get<std::string>() : slot.defaultName;
        slot.base->SetFullName(name.c_str());
        slot.base->shortName = name;
        if (assignments.contains(key)) Names::Apply(slot.base, slot.actor, name);
    }
}

void StudioService::AfterLoad(bool success) {
    loaded = success;
    if (success && ready && stateValid) {
        for (const auto& slot : slots) {
            if (!assignments.contains(std::to_string(slot.id)) && !slot.actor->IsDisabled()) {
                stateValid = false;
                error = "В сохранении есть NPCStudio NPC без назначения в co-save. Пул заблокирован, чтобы не занять чужой слот";
                logger::error("{}", error);
                break;
            }
        }
    }
    if (success) {
        RestoreNames();
        externalWork = stateValid && !externalLooks.empty();
        if (stateValid) for (auto& slot : slots)
            if (assignments.contains(std::to_string(slot.id)) && !slot.actor->IsDisabled()) QueueRestore(slot);
    }
}

void StudioService::NewGame() {
    lookFrame.reset();
    settlingTicks = 0;
    UI::SetWorldWork(false);
    Preview::Clear();
    assignments = json::object();
    externalLooks.clear(); externalWork = false; appearanceTarget.reset();
    usedSlots.clear(); restoring.clear(); workPending = false;
    stateValid = true;
    if (ready) error.clear();
    loaded = true;
    RestoreNames();
}

void StudioService::Revert() {
    // RE:: объекты здесь не изменяются: SKSE вызывает это и во время смены мира.
    ++worldEpoch; restoring.clear(); workPending = false;
    usedSlots.clear();
    loaded = false;
    stateValid = true;
    if (ready) error.clear();
    assignments = json::object();
    externalLooks.clear(); externalWork = false; appearanceTarget.reset();
    hairDraft = {};
    lookFrame.reset();
}

void StudioService::Save(SKSE::SerializationInterface* stream) {
    if (!ready || !stateValid) return;
    for (auto& slot : slots) {
        const auto key = std::to_string(slot.id);
        if (assignments.contains(key) && !slot.actor->IsDisabled() && slot.actor->Is3DLoaded() && !restoring.contains(slot.id)) assignments[key]["loadout"] = Equipment::Capture(slot.actor);
    }
    for (auto& slot : slots) {
        const auto key = std::to_string(slot.id);
        if (!assignments.contains(key) || restoring.contains(slot.id)) continue;
        auto parts = json::array();
        for (int i = 0; slot.base->headParts && i < slot.base->numHeadParts; ++i) {
            auto* part = slot.base->headParts[i];
            auto* file = part ? part->GetFile(0) : nullptr;
            if (file) parts.push_back({{"plugin", file->GetFilename()}, {"form", part->GetLocalFormID()}});
        }
        if (!parts.empty()) assignments[key]["headParts"] = std::move(parts);
        if (slot.base->faceData) {
            assignments[key]["faceMorphs"] = slot.base->faceData->morphs;
            assignments[key]["facePresets"] = slot.base->faceData->parts;
        }
    }
    auto external = json::array();
    for (const auto& [id, edit] : externalLooks) external.push_back({{"reference", id}, {"base", edit.base}, {"look", edit.look}});
    const auto text = json{{"poolVersion", 3}, {"assignments", assignments}, {"usedSlots", usedSlots}, {"externalAppearance", external}}.dump();
    if (!stream->WriteRecord(StateTag, StateVersion, text.data(), static_cast<std::uint32_t>(text.size())))
        logger::error("Не удалось сохранить состояние NPCStudio");
}

void StudioService::Load(SKSE::SerializationInterface* stream) {
    assignments = json::object();
    externalLooks.clear();
    std::uint32_t type, version, length;
    bool seen = false;
    try {
        while (stream->GetNextRecordInfo(type, version, length)) {
            if (type != StateTag) continue;
            if (seen || version != StateVersion || length > 32 * 1024 * 1024) throw std::runtime_error("Эта тестовая сборка требует новую игру. Старые сохранения NPCStudio не поддерживаются");
            seen = true;
            std::string text(length, '\0');
            if (stream->ReadRecordData(text.data(), length) != length) throw std::runtime_error("Оборванное состояние NPCStudio");
            auto saved = json::parse(text);
            auto decoded = DecodePoolState(saved);
            assignments = std::move(decoded.assignments);
            usedSlots = std::move(decoded.used);
            const auto external = saved.value("externalAppearance", json::array());
            if (external.is_array() && external.size() <= 128) for (const auto& row : external) {
                try {
                    RE::FormID id, base;
                    const auto& look = row.at("look");
                    if (!look.is_object() || !stream->ResolveFormID(row.at("reference").get<RE::FormID>(), id) ||
                        !stream->ResolveFormID(row.at("base").get<RE::FormID>(), base)) continue;
                    // Проверяем формат сохранённых ключей до передачи в цикл обновления.
                    Appearance::SavedHair(look);
                    Appearance::SavedColor(look);
                    externalLooks.emplace(id, ExternalLook{base, look});
                } catch (const std::exception& e) { logger::warn("Пропущена внешность NPC: {}", e.what()); }
            }
        }
    } catch (const std::exception& e) {
        stateValid = false;
        error = e.what();
        assignments = json::object();
        logger::error("Загрузка co-save остановлена: {}", error);
    }
}
}
