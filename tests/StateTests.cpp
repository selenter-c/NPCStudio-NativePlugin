#include "ui/SettingsCodec.h"
#include "core/StateCodec.h"
#include <iostream>
int main() {
    using nlohmann::json;
    const auto check = [](bool ok, const char *label) {
        if (!ok) {
            std::cerr << label << '\n';
            std::exit(1);
        }
    };
    const auto accepts = [](const json &value) {
        try {
            NPCStudio::DecodePoolState(value);
            return true;
        } catch (...) {
            return false;
        }
    };
    using NPCStudio::UI::DefaultSettings;
    using NPCStudio::UI::NormalizeSettings;
    const auto ru = DefaultSettings("ru"), en = DefaultSettings("de");
    check(ru.at("language") == "ru" && en.at("language") == "en", "Язык системы и английский fallback");
    auto settings = ru;
    settings["language"] = "en";
    settings["theme"] = "sapphire";
    settings["key"] = 66;
    check(NormalizeSettings(json::parse(settings.dump()), ru) == settings,
          "Выбранные язык и клавиши не переписываются");
    for (auto invalid : {json{{"key", 1}}, json{{"key", 42}}, json{{"modifiers", 8}}, json{{"language", "xx"}},
                         json{{"theme", "bad"}}}) {
        bool rejected = false;
        try {
            NormalizeSettings(invalid, ru);
        } catch (...) {
            rejected = true;
        }
        check(rejected, "Недопустимые настройки приняты");
    }
    auto state = json{{"poolVersion", 3},
                      {"assignments",
                       {{"0", {{"name", "Элара"}, {"identity", "first-life"}, {"race", "OrcRace"}}},
                        {"399", {{"name", "Рал"}, {"identity", "last-life"}, {"race", "WoodElfRace"}}}}},
                      {"usedSlots", {0, 399}}};
    check(accepts(state), "Кириллица, орк и крайние слоты");
    check(NPCStudio::DecodePoolState(json::parse(state.dump())).assignments == state.at("assignments"),
          "Текущий co-save изменён при чтении");
    for (unsigned id = 0; id < 400; ++id) {
        check(NPCStudio::FemaleSlot(id) == (id < 200), "Пул не разделён на два последовательных блока");
        check(NPCStudio::SlotNumber(id) == id % 200 + 1, "Номер слота не соответствует полу");
    }
    for (const auto *key : {"-1", "400", "01", "1x", "999999999999999999999"}) {
        auto broken = state;
        broken["assignments"][key] = state.at("assignments").at("0");
        check(!accepts(broken), "Некорректный номер слота принят");
    }
    for (const auto &name :
         {std::string(), std::string("   "), std::string("NPC\n"), std::string("A\0B", 3), std::string(161, 'a')}) {
        auto broken = state;
        broken["assignments"]["0"]["name"] = name;
        check(!accepts(broken), "Некорректное имя принято");
    }
    for (const auto *field : {"identity", "race"}) {
        auto broken = state;
        broken["assignments"]["0"].erase(field);
        check(!accepts(broken), "Обязательные данные подставлены автоматически");
        broken = state;
        broken["assignments"]["0"][field] = 7;
        check(!accepts(broken), "Неверный тип данных принят");
    }
    auto removed = state;
    removed["assignments"].erase("0");
    auto decoded = NPCStudio::DecodePoolState(json::parse(removed.dump()));
    check(decoded.assignments.size() == 1 && decoded.used.contains(0), "Удаление стерло историю слота");
    check(NPCStudio::SlotHistoryPriority(1, decoded) < NPCStudio::SlotHistoryPriority(0, decoded),
          "Повторный слот выбран раньше нового");
    auto reused = removed;
    reused["assignments"]["0"] = {{"name", "Новый NPC"}, {"identity", "second-life"}, {"race", "NordRace"}};
    check(accepts(reused), "Повторное использование слота не читается");
    auto broken = state;
    broken["usedSlots"] = {399};
    check(!accepts(broken), "Назначение без истории принято");
    for (auto history : {json{0, 0}, json{-1}, json{400}, json{"0"}}) {
        broken = state;
        broken["usedSlots"] = history;
        check(!accepts(broken), "Поврежденная история принята");
    }
    for (unsigned version : {1, 2, 4}) {
        broken = state;
        broken["poolVersion"] = version;
        check(!accepts(broken), "Другой формат сохранения принят");
    }
    check(accepts(json{{"poolVersion", 3}, {"assignments", json::object()}, {"usedSlots", json::array()}}),
          "Чистый пул не читается");
    check(!accepts(json::object()), "Пустой co-save принят");
    std::cout << "Current state/settings codec: passed\n";
}
