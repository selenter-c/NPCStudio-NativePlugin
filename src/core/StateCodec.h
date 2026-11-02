#pragma once
#include <nlohmann/json.hpp>
#include <charconv>
#include <stdexcept>
#include <string>
#include <set>

namespace NPCStudio {
inline constexpr unsigned SlotCount = 400;
inline bool FemaleSlot(unsigned id) { return id < 80 || (id >= 160 && id < 280); }
inline unsigned SlotNumber(unsigned id) { return id < 80 ? id + 1 : id < 160 ? id - 79 : id < 280 ? id - 79 : id - 199; }
inline bool ValidName(const std::string& name) {
    if (name.empty() || name.size() > 160) return false;
    bool hasText = false;
    for (const auto character : name) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 32 || byte == 127) return false;
        if (byte != 32) hasText = true;
    }
    return hasText;
}
struct PoolState {
    nlohmann::json assignments = nlohmann::json::object();
    std::set<unsigned> used;
    bool legacyHistoryUnknown = false;
};
inline PoolState DecodePoolState(const nlohmann::json& saved) {
    const auto version = saved.at("poolVersion").get<unsigned>();
    if ((version != 1 && version != 2) || !saved.at("assignments").is_object()) throw std::runtime_error("Неизвестная версия пула");
    PoolState result;
    result.assignments = saved.at("assignments");
    result.legacyHistoryUnknown = version == 1 || saved.value("legacyHistoryUnknown", false);
    if (result.assignments.size() > SlotCount) throw std::runtime_error("Слишком много слотов в co-save");
    for (const auto& [key, item] : result.assignments.items()) {
        unsigned id = SlotCount;
        const auto parse = std::from_chars(key.data(), key.data() + key.size(), id);
        if (parse.ec != std::errc() || parse.ptr != key.data() + key.size() || id >= (version == 1 ? 160u : SlotCount) || std::to_string(id) != key ||
            !item.at("name").is_string() || !ValidName(item.at("name").get<std::string>())) throw std::runtime_error("Повреждённые данные слота");
        if (item.contains("identity") && (!item.at("identity").is_string() || !ValidName(item.at("identity").get<std::string>()))) throw std::runtime_error("Повреждена личность слота");
        result.used.insert(id);
    }
    if (version == 2) {
        const auto& used = saved.at("usedSlots");
        if (!used.is_array() || used.size() > SlotCount) throw std::runtime_error("Повреждена история слотов");
        std::set<unsigned> seen;
        for (const auto& id : used) {
            if (!id.is_number_unsigned() && !id.is_number_integer()) throw std::runtime_error("Неверный слот истории");
            const auto value = id.get<std::int64_t>();
            if (value < 0 || value >= static_cast<int>(SlotCount) || !seen.insert(static_cast<unsigned>(value)).second) throw std::runtime_error("Неверная история слотов");
            result.used.insert(static_cast<unsigned>(value));
        }
    }
    return result;
}
inline nlohmann::json DecodeState(const nlohmann::json& saved) { return DecodePoolState(saved).assignments; }
inline int SlotHistoryPriority(unsigned id, const PoolState& state) {
    if (state.used.contains(id)) return 2;
    return state.legacyHistoryUnknown && id < 160 ? 1 : 0;
}
}
