#include "core/EquipmentService.h"
#include "core/StateCodec.h"
#include <fstream>
#include <map>
#include <unordered_set>

namespace NPCStudio::Equipment {
using json = nlohmann::json;
static const std::filesystem::path PresetDirectory = "Data/SKSE/Plugins/NPCStudio/Equipment";

static bool Supported(RE::TESForm* form) {
    return form && (form->Is(RE::FormType::Armor) || form->Is(RE::FormType::Weapon) || form->Is(RE::FormType::Ammo));
}

static json Address(RE::TESForm* form) {
    auto* file = form->GetFile(0);
    if (!file || (form->GetFormID() >> 24) == 0xFF) throw std::runtime_error("Созданный игрой предмет нельзя сохранить в переносимый набор");
    return {{"plugin", file->GetFilename()}, {"form", form->GetLocalFormID()}};
}

static RE::TESBoundObject* Resolve(const json& item) {
    const auto plugin = item.at("plugin").get<std::string>();
    const auto id = item.at("form").get<std::uint32_t>();
    if (plugin.empty() || plugin.size() > 260 || plugin.find_first_of("/\\:") != std::string::npos || id > 0xFFFFFF)
        throw std::runtime_error("Неверный адрес предмета в наборе");
    auto* form = RE::TESDataHandler::GetSingleton()->LookupForm(id, plugin);
    if (!Supported(form)) throw std::runtime_error(fmt::format("Предмет {}|{:06X} отсутствует или имеет неподдерживаемый тип", plugin, id));
    return form->As<RE::TESBoundObject>();
}

static std::map<std::string, json> ReadPresets() {
    std::map<std::string, json> result;
    if (!std::filesystem::exists(PresetDirectory)) return result;
    for (const auto& entry : std::filesystem::directory_iterator(PresetDirectory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        try {
            if (entry.file_size() > 65536) throw std::runtime_error("Файл больше 64 КБ");
            std::ifstream file(entry.path());
            auto preset = json::parse(file);
            if (preset.at("schemaVersion") != 1 || !ValidName(preset.at("name").get<std::string>()) ||
                !preset.at("items").is_array() || preset.at("items").empty() || preset.at("items").size() > 64)
                throw std::runtime_error("Неверный формат набора");
            result[entry.path().stem().string()] = std::move(preset);
        } catch (const std::exception& e) { logger::warn("Набор {} пропущен: {}", entry.path().string(), e.what()); }
    }
    return result;
}

static std::wstring Fold(std::string_view text) {
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
    if (!wide.empty()) CharLowerBuffW(wide.data(), static_cast<DWORD>(wide.size()));
    return wide;
}

static json ItemInfo(RE::TESBoundObject* form, int count, bool worn) {
    auto item = Address(form);
    item["name"] = form->GetName();
    item["count"] = count;
    item["worn"] = worn;
    item["kind"] = form->Is(RE::FormType::Armor) ? "armor" : form->Is(RE::FormType::Weapon) ? "weapon" : "ammo";
    return item;
}

json Describe(RE::Actor* actor, const json& command) {
    json inventory = json::array(), presets = json::array(), catalog = json::array();
    for (auto& [object, entry] : actor->GetInventory()) {
        if (!Supported(object) || entry.first <= 0) continue;
        try { inventory.push_back(ItemInfo(object, entry.first, entry.second && entry.second->IsWorn())); }
        catch (const std::exception&) { /* Динамические предметы не входят в переносимые наборы. */ }
    }
    for (const auto& [id, preset] : ReadPresets()) {
        auto preview = preset;
        preview["id"] = id;
        preview["available"] = true;
        preview["error"] = "";
        try { for (const auto& item : preset.at("items")) Resolve(item); }
        catch (const std::exception& e) { preview["available"] = false; preview["error"] = e.what(); }
        presets.push_back(std::move(preview));
    }
    const auto query = Fold(command.value("query", std::string()));
    if (query.size() > 80) throw std::runtime_error("Слишком длинный поисковый запрос");
    const auto kind = command.value("kind", std::string("armor"));
    auto type = kind == "weapon" ? RE::FormType::Weapon : kind == "ammo" ? RE::FormType::Ammo : RE::FormType::Armor;
    auto& forms = RE::TESDataHandler::GetSingleton()->GetFormArray(type);
    unsigned matches = 0;
    for (auto* form : forms) {
        if (!Supported(form) || !form->GetPlayable() || !form->GetName() || !*form->GetName()) continue;
        auto* file = form->GetFile(0);
        if (!file || (form->GetFormID() >> 24) == 0xFF) continue;
        if (!query.empty() && Fold(std::string(form->GetName()) + " " + std::string(file->GetFilename())).find(query) == std::wstring::npos) continue;
        ++matches;
        if (catalog.size() < 100) catalog.push_back(ItemInfo(form->As<RE::TESBoundObject>(), 1, false));
    }
    return {{"inventory", inventory}, {"presets", presets}, {"catalog", catalog}, {"matches", matches}};
}

static void RequireActive(RE::Actor* actor) {
    if (actor->IsDisabled() || actor->IsDead() || !actor->Is3DLoaded())
        throw std::runtime_error("Сначала призовите живого NPC рядом с собой и дождитесь его появления");
}

static void GiveAndEquip(RE::Actor* actor, RE::TESBoundObject* object, unsigned count) {
    auto inventory = actor->GetInventory();
    auto it = inventory.find(object);
    const int existing = it == inventory.end() ? 0 : std::max(0, it->second.first);
    if (existing < static_cast<int>(count)) actor->AddObjectToContainer(object, nullptr, static_cast<int>(count) - existing, nullptr);
    // Без forceEquip: предмет впоследствии можно снять обычными средствами.
    RE::ActorEquipManager::GetSingleton()->EquipObject(actor, object, nullptr, count, nullptr, false, false, false, true);
}

void Execute(RE::Actor* actor, const json& command) {
    const auto action = command.at("action").get<std::string>();
    if (action == "equipment") return;
    RequireActive(actor);
    if (action == "giveItem") {
        auto* object = Resolve(command.at("item"));
        const unsigned count = command.value("count", 1u);
        if (!count || count > 1000) throw std::runtime_error("Количество должно быть от 1 до 1000");
        GiveAndEquip(actor, object, object->Is(RE::FormType::Ammo) ? count : 1);
    } else if (action == "unequipItem") {
        auto* object = Resolve(command.at("item"));
        auto inventory = actor->GetInventory();
        auto it = inventory.find(object);
        if (it == inventory.end() || it->second.first <= 0 || !it->second.second) throw std::runtime_error("Предмета уже нет у NPC");
        auto* extras = it->second.second->extraLists;
        if (extras) for (auto* extra : *extras) {
            if (extra && (extra->HasType<RE::ExtraWorn>() || extra->HasType<RE::ExtraWornLeft>()))
                RE::ActorEquipManager::GetSingleton()->UnequipObject(actor, object, extra, 1, nullptr, false, false, false, true);
        }
    } else if (action == "applyEquipmentPreset") {
        const auto presets = ReadPresets();
        const auto found = presets.find(command.at("preset").get<std::string>());
        if (found == presets.end()) throw std::runtime_error("Набор не найден");
        std::vector<std::pair<RE::TESBoundObject*, unsigned>> items;
        std::unordered_set<RE::TESBoundObject*> seen;
        // Все зависимости проверяются до первого изменения инвентаря.
        for (const auto& item : found->second.at("items")) {
            auto* object = Resolve(item);
            const auto count = item.value("count", 1u);
            if (!count || count > 1000 || !seen.insert(object).second) throw std::runtime_error("Неверное количество или повтор предмета в наборе");
            items.emplace_back(object, object->Is(RE::FormType::Ammo) ? count : 1);
        }
        for (auto& [object, entry] : actor->GetInventory()) {
            if (!Supported(object) || entry.first <= 0 || !entry.second || !entry.second->IsWorn()) continue;
            auto* extras = entry.second->extraLists;
            if (extras) for (auto* extra : *extras) {
                if (extra && (extra->HasType<RE::ExtraWorn>() || extra->HasType<RE::ExtraWornLeft>()))
                    RE::ActorEquipManager::GetSingleton()->UnequipObject(actor, object, extra, 1, nullptr, false, false, false, true);
            }
        }
        for (auto [object, count] : items) GiveAndEquip(actor, object, count);
        logger::info("Набор '{}' применён к {:08X}", found->first, actor->GetFormID());
    } else if (action == "saveEquipmentPreset") {
        const auto name = command.at("name").get<std::string>();
        if (!ValidName(name)) throw std::runtime_error("Некорректное имя набора");
        json items = json::array();
        for (auto& [object, entry] : actor->GetInventory()) {
            if (!Supported(object) || entry.first <= 0 || !entry.second || !entry.second->IsWorn()) continue;
            auto item = Address(object);
            item["count"] = object->Is(RE::FormType::Ammo) ? std::min(entry.first, 1000) : 1;
            items.push_back(std::move(item));
        }
        if (items.empty() || items.size() > 64) throw std::runtime_error("Наденьте от 1 до 64 предметов перед сохранением набора");
        std::filesystem::create_directories(PresetDirectory);
        auto path = PresetDirectory / fmt::format("user-{}-{}.json", GetTickCount64(), GetCurrentProcessId());
        auto temp = path; temp += ".tmp";
        {
            std::ofstream file(temp, std::ios::binary | std::ios::trunc);
            file << json{{"schemaVersion", 1}, {"name", name}, {"items", items}}.dump(2);
            file.flush();
            if (!file) throw std::runtime_error("Не удалось записать набор на диск");
        }
        if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Не удалось завершить сохранение набора");
    } else throw std::runtime_error("Неизвестная команда снаряжения");
}
}
