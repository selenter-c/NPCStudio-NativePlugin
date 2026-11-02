#pragma once
#include <nlohmann/json.hpp>
#include <charconv>
#include <stdexcept>
#include <string>

namespace NPCStudio {
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

inline nlohmann::json DecodeState(const nlohmann::json& saved) {
    if (saved.at("poolVersion") != 1 || !saved.at("assignments").is_object())
        throw std::runtime_error("Неизвестная версия пула");
    auto result = saved.at("assignments");
    if (result.size() > 160) throw std::runtime_error("Слишком много слотов в co-save");
    for (const auto& [key, item] : result.items()) {
        unsigned id = 160;
        const auto parse = std::from_chars(key.data(), key.data() + key.size(), id);
        if (parse.ec != std::errc() || parse.ptr != key.data() + key.size() || id >= 160 || std::to_string(id) != key ||
            !item.at("name").is_string() || !ValidName(item.at("name").get<std::string>()))
            throw std::runtime_error("Повреждённые данные слота");
        if (item.contains("identity") && (!item.at("identity").is_string() || !ValidName(item.at("identity").get<std::string>())))
            throw std::runtime_error("Повреждена личность слота");
    }
    return result;
}
}
