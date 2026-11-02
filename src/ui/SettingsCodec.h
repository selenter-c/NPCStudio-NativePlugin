#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
namespace NPCStudio::UI {
inline nlohmann::json DefaultSettings(const std::string &systemLanguage) {
    return {{"schema", 2},
            {"key", 9},
            {"modifiers", 1},
            {"language", systemLanguage == "ru" ? "ru" : "en"},
            {"theme", "amethyst"}};
}
inline nlohmann::json NormalizeSettings(const nlohmann::json &input, const nlohmann::json &fallback) {
    const auto key = input.value("key", fallback.at("key").get<unsigned>());
    const auto modifiers = input.value("modifiers", fallback.at("modifiers").get<unsigned>());
    const auto language = input.value("language", fallback.at("language").get<std::string>());
    const auto theme = input.value("theme", fallback.at("theme").get<std::string>());
    if (key < 2 || key > 211 || modifiers > 7 || key == 0x2A || key == 0x36 || key == 0x1D || key == 0x9D ||
        key == 0x38 || key == 0xB8)
        throw std::runtime_error("Неверная клавиша или модификатор");
    if (language != "ru" && language != "en")
        throw std::runtime_error("Unsupported language");
    if (theme != "amethyst" && theme != "emerald" && theme != "sapphire" && theme != "ember")
        throw std::runtime_error("Unsupported theme");
    return {{"schema", 2}, {"key", key}, {"modifiers", modifiers}, {"language", language}, {"theme", theme}};
}
} // namespace NPCStudio::UI
