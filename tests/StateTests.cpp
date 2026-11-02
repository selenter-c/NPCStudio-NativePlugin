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
    state["poolVersion"] = 2;
    check(!accepts(state), "Неизвестная версия принята");
    check(!accepts(json::object()), "Пустой co-save принят");
    std::cout << "State codec: passed\n";
}
