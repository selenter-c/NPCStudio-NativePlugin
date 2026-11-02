#include "ui/SettingsCodec.h"
#include "core/StateCodec.h"
#include <iostream>

int main() {
    using nlohmann::json;
    const auto accepts = [](const json& value) {
        try { NPCStudio::DecodeState(value); return true; }
        catch (...) { return false; }
    };
    const auto check = [](bool ok, const char* label) {
        if (!ok) { std::cerr << label << '\n'; std::exit(1); }
    };
    using NPCStudio::UI::DefaultSettings;
    using NPCStudio::UI::NormalizeSettings;
    const auto ru = DefaultSettings("ru"), en = DefaultSettings("de");
    check(ru.at("language") == "ru" && en.at("language") == "en", "Первый язык системы и английский fallback");
    auto custom = ru; custom["language"] = "en"; custom["theme"] = "sapphire";
    check(NormalizeSettings(json::parse(custom.dump()), ru, true) == custom, "Выбранный язык сохраняется независимо от системы");
    const auto oldSettings = json{{"key",66},{"modifiers",1}};
    check(NormalizeSettings(oldSettings,ru,true).at("key") == 9, "Старый стандартный Shift+F8 мигрирует в Shift+8");
    auto customKey = oldSettings; customKey["key"] = 65;
    check(NormalizeSettings(customKey,ru,true).at("key") == 65, "Переназначенная пользователем клавиша сохраняется");
    auto modernKey = oldSettings; modernKey["schema"] = 2;
    check(NormalizeSettings(modernKey,ru,true).at("key") == 66, "Явно выбранный F8 в новой версии сохраняется");
    for (auto invalid : {json{{"key",1}}, json{{"key",42}}, json{{"modifiers",8}}, json{{"language","xx"}}, json{{"theme","bad"}}}) {
        bool rejected = false;
        try { NormalizeSettings(invalid,ru); } catch (...) { rejected = true; }
        check(rejected,"Недопустимые настройки отвергаются до записи");
    }
    auto state = json{{"poolVersion", 1}, {"assignments", {{"0", {{"name", "Элара"}}}, {"159", {{"name", "Рал"}}}}}};
    check(accepts(state), "Кириллица и крайние слоты");
    check(NPCStudio::DecodeState(state) == state["assignments"], "Состояние не изменяется при чтении");
    for (const auto* key : {"-1", "160", "01", "1x", "999999999999999999999"}) {
        auto broken = state;
        broken["assignments"][key] = {{"name", "NPC"}};
        check(!accepts(broken), "Некорректный номер слота принят");
    }
    for (const auto& name : {std::string(), std::string("   "), std::string("NPC\n"), std::string("A\0B", 3), std::string(161, 'a')}) {
        auto broken = state;
        broken["assignments"]["0"]["name"] = name;
        check(!accepts(broken), "Некорректное имя принято");
    }
    auto removed = state;
    removed["assignments"].erase("0");
    check(NPCStudio::DecodeState(json::parse(removed.dump())) == removed["assignments"], "Удалённый слот возвращается после сериализации");
    removed["assignments"]["0"] = {{"name", "Новый NPC"}, {"identity", "slot-0-new-life"}};
    check(accepts(removed), "Повторное назначение слота не читается");
    auto badIdentity = removed;
    badIdentity["assignments"]["0"]["identity"] = 7;
    check(!accepts(badIdentity), "Повреждённая личность слота принята");
    auto migrated = NPCStudio::DecodePoolState(state);
    check(migrated.legacyHistoryUnknown && migrated.used.contains(159), "История старого сохранения потеряна");
    check(NPCStudio::SlotHistoryPriority(160, migrated) < NPCStudio::SlotHistoryPriority(1, migrated), "Новый слот не имеет приоритета перед неизвестным");
    auto expanded = json{{"poolVersion", 2}, {"assignments", {{"399", {{"name", "Крайний слот"}, {"identity", "new"}}}}}, {"usedSlots", {0, 399}}, {"legacyHistoryUnknown", false}};
    auto decoded = NPCStudio::DecodePoolState(json::parse(expanded.dump()));
    check(decoded.used.contains(0) && decoded.used.contains(399), "Удаление не должно стирать историю");
    check(NPCStudio::SlotHistoryPriority(1, decoded) < NPCStudio::SlotHistoryPriority(0, decoded), "Повторный слот предпочитается новому");
    check(NPCStudio::FemaleSlot(159) == false && NPCStudio::FemaleSlot(160) && NPCStudio::FemaleSlot(279) && !NPCStudio::FemaleSlot(280), "Пол новых и старых слотов нарушен");
    check(NPCStudio::SlotNumber(160) == 81 && NPCStudio::SlotNumber(399) == 200, "Неверный номер в резерве");
    auto badHistory = expanded; badHistory["usedSlots"] = {0, 0}; check(!accepts(badHistory), "Дубликат истории принят");
    badHistory["usedSlots"] = {-1}; check(!accepts(badHistory), "Отрицательный слот истории принят");
    badHistory["usedSlots"] = {400}; check(!accepts(badHistory), "Выход за резерв принят");
    state["poolVersion"] = 3;
    check(!accepts(state), "Неизвестная версия принята");
    check(!accepts(json::object()), "Пустой co-save принят");
    std::cout << "State codec: passed\n";
}
