#pragma once
#include <nlohmann/json.hpp>
namespace NPCStudio::Preview {
// РљРѕРјР°РЅРґС‹ РІС‹РїРѕР»РЅСЏСЋС‚СЃСЏ РІ РёРіСЂРѕРІРѕРј РїРѕС‚РѕРєРµ; callback РїСЂРѕРІР°Р№РґРµСЂР° РЅРµ РѕР±СЂР°С‰Р°РµС‚СЃСЏ Рє RE.
nlohmann::json Frame(RE::Actor* actor, const std::string& identity, const nlohmann::json& options);
nlohmann::json ItemFrame(RE::TESBoundObject* item, const nlohmann::json& options);
nlohmann::json HairThumbnail(RE::BGSHeadPart* part, unsigned token, std::uint64_t fingerprint);
void CancelHairThumbnail(unsigned token);
bool IsCurrent(std::uint64_t generation);
void Clear();
std::string ReadPortrait(const std::string& identity);
void SavePortrait(const std::string& identity, const std::string& image);
} // namespace NPCStudio::Preview
