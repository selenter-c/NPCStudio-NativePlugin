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
