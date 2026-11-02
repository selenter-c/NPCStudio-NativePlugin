#include "core/StudioService.h"
#include "core/StateCodec.h"
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
        if (manifest.at("schemaVersion") != 1 || manifest.at("plugin") != "NPCStudio.esp" || manifest.at("slots").size() != 160)
            throw std::runtime_error("Неверный манифест пула NPCStudio");
        auto* data = RE::TESDataHandler::GetSingleton();
        std::unordered_set<RE::FormID> bases, actors;
        std::vector<Slot> resolved;
        for (const auto& entry : manifest.at("slots")) {
            Slot slot;
            slot.id = entry.at("id").get<unsigned>();
            slot.female = entry.at("female").get<bool>();
            slot.base = data->LookupForm<RE::TESNPC>(entry.at("base").get<unsigned>(), "NPCStudio.esp");
            slot.actor = data->LookupForm<RE::Actor>(entry.at("reference").get<unsigned>(), "NPCStudio.esp");
            if (slot.id != resolved.size() || !slot.base || !slot.actor || slot.actor->GetActorBase() != slot.base ||
                slot.base->IsFemale() != slot.female || !bases.insert(slot.base->GetFormID()).second ||
                !actors.insert(slot.actor->GetFormID()).second)
                throw std::runtime_error("ESP и манифест слотов не совпадают");
            slot.defaultName = slot.base->GetName();
            resolved.push_back(std::move(slot));
        }
        slots = std::move(resolved);
        ready = true;
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
    json list = json::array();
    unsigned females = 0, males = 0;
    for (const auto& slot : slots) {
        const auto key = std::to_string(slot.id);
        if (!assignments.contains(key)) continue;
        const auto& saved = assignments.at(key);
        slot.female ? ++females : ++males;
        list.push_back({{"id", slot.id}, {"female", slot.female}, {"name", saved.at("name")},
            {"parked", slot.actor->IsDisabled()}, {"dead", slot.actor->IsDead()},
            {"reference", fmt::format("{:08X}", slot.actor->GetFormID())},
            {"race", slot.base->GetRace() ? slot.base->GetRace()->GetName() : ""}});
    }
    return {{"version", "0.2.0"}, {"ready", ready && stateValid}, {"worldLoaded", loaded}, {"error", error},
        {"femaleUsed", females}, {"maleUsed", males}, {"slots", list},
        {"capabilities", {{"preview", false}, {"appearance", false}, {"biography", false}}}};
}

json StudioService::Execute(const json& command) {
    const auto action = command.at("action").get<std::string>();
    if (action == "state") return Snapshot();
    if (!ready || !stateValid) throw std::runtime_error(error.empty() ? "Пул недоступен" : error);
    if (!loaded || !RE::PlayerCharacter::GetSingleton()->GetParentCell())
        throw std::runtime_error("Сначала загрузите сохранение или начните новую игру");
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (action == "create") {
        const bool female = command.at("female").get<bool>();
        auto name = command.at("name").get<std::string>();
        if (!ValidName(name))
            throw std::runtime_error("Имя должно содержать от 1 до 160 байт без переноса строки");
        auto it = std::find_if(slots.begin(), slots.end(), [&](const Slot& s) {
            return s.female == female && !assignments.contains(std::to_string(s.id));
        });
        if (it == slots.end()) throw std::runtime_error("Свободных слотов выбранного пола нет");
        auto& slot = *it;
        if (slot.actor->IsDead()) throw std::runtime_error("Резервный актёр погиб: слот требует диагностики");
        slot.base->SetFullName(name.c_str());
        slot.actor->SetDisplayName(RE::BSFixedString(name), true);
        slot.actor->MoveTo(player);
        slot.actor->Enable(false);
        assignments[std::to_string(slot.id)] = {{"name", name}, {"identity", fmt::format("slot-{}", slot.id)}};
        logger::info("Создан NPC '{}' в слоте {}, reference {:08X}", name, slot.id, slot.actor->GetFormID());
    } else {
        auto& slot = Find(command.at("id").get<unsigned>());
        const auto key = std::to_string(slot.id);
        if (!assignments.contains(key)) throw std::runtime_error("Слот ещё не занят");
        if (action == "summon") {
            if (slot.actor->IsDead()) throw std::runtime_error("NPC погиб. Призыв не воскрешает персонажа");
            slot.actor->MoveTo(player);
            slot.actor->Enable(false);
        } else if (action == "park") {
            // Слот остаётся занятым: его личность и сохранённые данные не переиспользуются.
            slot.actor->Disable();
        } else if (action == "teleport") {
            if (slot.actor->IsDisabled()) throw std::runtime_error("NPC убран из мира. Сначала призовите его");
            player->MoveTo(slot.actor);
        } else if (action == "rename") {
            auto name = command.at("name").get<std::string>();
            if (!ValidName(name))
                throw std::runtime_error("Некорректное имя");
            slot.base->SetFullName(name.c_str());
            slot.actor->SetDisplayName(RE::BSFixedString(name), true);
            assignments[key]["name"] = name;
        } else throw std::runtime_error("Неизвестная команда");
    }
    return Snapshot();
}

void StudioService::BeforeLoad() { loaded = false; }

void StudioService::RestoreNames() {
    if (!ready || !stateValid) return;
    for (auto& slot : slots) {
        auto key = std::to_string(slot.id);
        const auto name = assignments.contains(key) ? assignments.at(key).at("name").get<std::string>() : slot.defaultName;
        slot.base->SetFullName(name.c_str());
        if (assignments.contains(key)) slot.actor->SetDisplayName(RE::BSFixedString(name), true);
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
    if (success) RestoreNames();
}

void StudioService::NewGame() {
    assignments = json::object();
    stateValid = true;
    loaded = true;
    RestoreNames();
}

void StudioService::Revert() {
    // RE:: объекты здесь не изменяются: SKSE вызывает это и во время смены мира.
    loaded = false;
    stateValid = true;
    assignments = json::object();
}

void StudioService::Save(SKSE::SerializationInterface* stream) {
    if (!ready || !stateValid) return;
    const auto text = json{{"poolVersion", 1}, {"assignments", assignments}}.dump();
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
            if (seen || version != StateVersion || length > 1024 * 1024) throw std::runtime_error("Неподдерживаемый co-save NPCStudio");
            seen = true;
            std::string text(length, '\0');
            if (stream->ReadRecordData(text.data(), length) != length) throw std::runtime_error("Оборванное состояние NPCStudio");
            auto saved = json::parse(text);
            assignments = DecodeState(saved);
        }
    } catch (const std::exception& e) {
        stateValid = false;
        error = e.what();
        assignments = json::object();
        logger::error("Загрузка co-save остановлена: {}", error);
    }
}
}
