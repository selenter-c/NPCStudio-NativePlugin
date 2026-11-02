#include "core/StudioService.h"
#include "core/StateCodec.h"
#include "core/EquipmentService.h"
#include "core/NameCompatibility.h"
#include "ui/MagelightBridge.h"
#include <fstream>
#include <unordered_set>

namespace NPCStudio {
using json = nlohmann::json;
static constexpr std::uint32_t StateTag = 0x5354534E;
static constexpr std::uint32_t StateVersion = 1;

StudioService& StudioService::Get() {
    static StudioService instance;
    return instance;
}

void StudioService::Initialize() {
    try {
        std::ifstream file("Data/SKSE/Plugins/NPCStudio/slots.json");
        auto manifest = json::parse(file);
        if (manifest.at("schemaVersion") != 2 || manifest.at("plugin") != "NPCStudio.esp" || manifest.at("slots").size() != SlotCount)
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
            Names::Protect(slot.base);
            resolved.push_back(std::move(slot));
        }
        slots = std::move(resolved);
        ready = true;
        worker = std::jthread([this](std::stop_token stop) {
            while (!stop.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                if (!workPending.load() || taskQueued.exchange(true)) continue;
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
                {"history", usedSlots.contains(slot.id) ? "used" : legacyHistoryUnknown && slot.id < 160 ? "unknown" : "fresh"}});
            continue;
        }
        const auto& saved = assignments.at(key);
        slot.female ? ++females : ++males;
        list.push_back({{"id", slot.id}, {"female", slot.female}, {"number", SlotNumber(slot.id)}, {"warning", saved.value("warning", std::string())}, {"audit", saved.value("audit", json::object())}, {"restoring", restoring.contains(slot.id)}, {"name", saved.at("name")}, {"identity", saved.value("identity", fmt::format("slot-{}", slot.id))},
            {"parked", slot.actor->IsDisabled()}, {"dead", slot.actor->IsDead()},
            {"reference", fmt::format("{:08X}", slot.actor->GetFormID())},
            {"race", slot.base->GetRace() ? slot.base->GetRace()->GetName() : ""}});
    }
    return {{"version", "0.4.0"}, {"ready", ready && stateValid}, {"worldLoaded", loaded}, {"error", error},
        {"femaleUsed", females}, {"maleUsed", males}, {"slots", list}, {"availableSlots", available}, {"capacity", 200}, {"library", Equipment::Library()}, {"settings", UI::Settings()},
        {"capabilities", {{"preview", false}, {"equipment", true}, {"appearance", false}, {"biography", false}}}};
}

json StudioService::Execute(const json& command) {
    const auto action = command.at("action").get<std::string>();
    if (action == "state") return Snapshot();
    if (action == "saveSettings") { UI::SaveSettings(command.at("settings")); return Snapshot(); }
    if (action == "addCategory") { Equipment::AddCategory(command.at("name").get<std::string>()); return Snapshot(); }
    if (!ready || !stateValid) throw std::runtime_error(error.empty() ? "Пул недоступен" : error);
    if (!loaded || !RE::PlayerCharacter::GetSingleton()->GetParentCell())
        throw std::runtime_error("Сначала загрузите сохранение или начните новую игру");
    auto* player = RE::PlayerCharacter::GetSingleton();
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
        auto name = Names::Random(female, base && base->GetRace() ? base->GetRace()->GetFormEditorID() : "NordRace");
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
            PoolState history{assignments, usedSlots, legacyHistoryUnknown};
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
        Names::Apply(slot.base, slot.actor, name);
        slot.actor->MoveTo(player);
        slot.actor->Enable(false);
        assignments[std::to_string(slot.id)] = {{"name", name}, {"identity", fmt::format("slot-{}-{}-{}", slot.id, std::chrono::system_clock::now().time_since_epoch().count(), GetCurrentProcessId())}};
        usedSlots.insert(slot.id);
        if (!preset.empty()) QueueRestore(slot, preset);
        logger::info("Создан NPC '{}' в слоте {}, reference {:08X}", name, slot.id, slot.actor->GetFormID());
    } else {
        auto& slot = Find(command.at("id").get<unsigned>());
        const auto key = std::to_string(slot.id);
        if (!assignments.contains(key)) throw std::runtime_error("Слот ещё не занят");
        const auto identity = assignments.at(key).value("identity", fmt::format("slot-{}", slot.id));
        if (command.contains("identity") && command.at("identity") != identity)
            throw std::runtime_error("Персонаж в этом слоте уже изменился. Обновите список");
        if (action == "delete") {
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
            slot.actor->Disable();
        } else if (action == "teleport") {
            if (slot.actor->IsDisabled()) throw std::runtime_error("NPC убран из мира. Сначала призовите его");
            player->MoveTo(slot.actor);
        } else if (action == "rename") {
            auto name = command.at("name").get<std::string>();
            if (!ValidName(name))
                throw std::runtime_error("Некорректное имя");
            Names::Apply(slot.base, slot.actor, name);
            assignments[key]["name"] = name;
        } else if (action == "equipment" || action == "giveItem" || action == "unequipItem" ||
            action == "applyEquipmentPreset" || action == "saveEquipmentPreset") {
            if (restoring.contains(slot.id)) throw std::runtime_error("Снаряжение ещё восстанавливается");
            Equipment::Execute(slot.actor, command);
            if (action != "equipment") assignments[key]["loadout"] = Equipment::Capture(slot.actor);
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
    slot.actor->MoveTo(holdingAnchor);
    Names::Apply(slot.base, slot.actor, slot.defaultName);
    const auto oldName = assignments.at(std::to_string(slot.id)).at("name").get<std::string>();
    assignments.erase(std::to_string(slot.id));
    logger::info("Удалён NPC '{}' из слота {}; reference {:08X} возвращена в резерв", oldName, slot.id, slot.actor->GetFormID());
}

void StudioService::QueueRestore(Slot& slot, const std::string& preset) {
    const auto key = std::to_string(slot.id);
    if (!preset.empty()) assignments[key]["pendingPreset"] = preset;
    restoring[slot.id] = {assignments.at(key).value("identity", fmt::format("slot-{}", slot.id)), assignments.at(key).value("pendingPreset", std::string())};
    assignments[key]["warning"] = "Восстановление снаряжения…";
    workPending = true;
}
void StudioService::Pump() {
    if (!loaded || !stateValid) { restoring.clear(); workPending = false; return; }
    for (auto it = restoring.begin(); it != restoring.end();) {
        auto& slot = Find(it->first);
        auto& job = it->second;
        const auto key = std::to_string(slot.id);
        if (!assignments.contains(key) || assignments.at(key).value("identity", fmt::format("slot-{}", slot.id)) != job.identity || slot.actor->IsDisabled()) { it = restoring.erase(it); continue; }
        try {
            if (++job.attempts > 40) throw std::runtime_error("Модель не загрузилась за 6 секунд; призовите NPC повторно");
            if (!slot.actor->Is3DLoaded()) { ++it; continue; }
            if (job.stage > 0 && job.stage++ < 4) { ++it; continue; }
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
            } else assignments[key]["warning"] = "Для старого скрытого NPC нет снимка надетых вещей. Примените набор и сохраните игру";
            it = restoring.erase(it);
        } catch (const std::exception& e) { assignments[key]["warning"] = e.what(); logger::warn("Восстановление слот {}: {}", slot.id, e.what()); it = restoring.erase(it); }
    }
    workPending = !restoring.empty();
}
void StudioService::BeforeLoad() { ++worldEpoch; restoring.clear(); workPending = false; loaded = false; }

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
        for (auto& slot : slots) if (assignments.contains(std::to_string(slot.id)) && !slot.actor->IsDisabled() && (assignments.at(std::to_string(slot.id)).contains("loadout") || assignments.at(std::to_string(slot.id)).contains("pendingPreset"))) QueueRestore(slot);
    }
}

void StudioService::NewGame() {
    assignments = json::object();
    usedSlots.clear(); legacyHistoryUnknown = false; restoring.clear(); workPending = false;
    stateValid = true;
    if (ready) error.clear();
    loaded = true;
    RestoreNames();
}

void StudioService::Revert() {
    // RE:: объекты здесь не изменяются: SKSE вызывает это и во время смены мира.
    ++worldEpoch; restoring.clear(); workPending = false;
    usedSlots.clear(); legacyHistoryUnknown = false;
    loaded = false;
    stateValid = true;
    if (ready) error.clear();
    assignments = json::object();
}

void StudioService::Save(SKSE::SerializationInterface* stream) {
    if (!ready || !stateValid) return;
    for (auto& slot : slots) {
        const auto key = std::to_string(slot.id);
        if (assignments.contains(key) && !slot.actor->IsDisabled() && slot.actor->Is3DLoaded() && !restoring.contains(slot.id)) assignments[key]["loadout"] = Equipment::Capture(slot.actor);
    }
    const auto text = json{{"poolVersion", 2}, {"assignments", assignments}, {"usedSlots", usedSlots}, {"legacyHistoryUnknown", legacyHistoryUnknown}}.dump();
    if (!stream->WriteRecord(StateTag, StateVersion, text.data(), static_cast<std::uint32_t>(text.size())))
        logger::error("Не удалось сохранить состояние NPCStudio");
}

void StudioService::Load(SKSE::SerializationInterface* stream) {
    assignments = json::object();
    std::uint32_t type, version, length;
    bool seen = false;
    try {
        while (stream->GetNextRecordInfo(type, version, length)) {
            if (type != StateTag) continue;
            if (seen || version != StateVersion || length > 32 * 1024 * 1024) throw std::runtime_error("Неподдерживаемый co-save NPCStudio");
            seen = true;
            std::string text(length, '\0');
            if (stream->ReadRecordData(text.data(), length) != length) throw std::runtime_error("Оборванное состояние NPCStudio");
            auto saved = json::parse(text);
            auto decoded = DecodePoolState(saved);
            assignments = std::move(decoded.assignments);
            usedSlots = std::move(decoded.used);
            legacyHistoryUnknown = decoded.legacyHistoryUnknown;
        }
    } catch (const std::exception& e) {
        stateValid = false;
        error = e.what();
        assignments = json::object();
        logger::error("Загрузка co-save остановлена: {}", error);
    }
}
}
