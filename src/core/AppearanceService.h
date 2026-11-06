#pragma once
#include <nlohmann/json.hpp>

namespace NPCStudio::Appearance {
struct Draft {
    RE::ObjectRefHandle target;
    RE::BGSHeadPart* originalHair = nullptr;
    RE::BGSHeadPart* hair = nullptr;
    RE::BGSColorForm* originalColor = nullptr;
    RE::BGSColorForm* color = nullptr;
};
nlohmann::json Describe(RE::Actor* actor, bool managed, const nlohmann::json& saved);
nlohmann::json Height(RE::Actor* actor);
void SetHeight(RE::Actor* actor, bool managed, const nlohmann::json& percent);
bool TryOn(RE::Actor* actor, bool managed, const nlohmann::json& saved, const nlohmann::json& command, Draft& draft);
nlohmann::json Commit(RE::Actor* actor, bool managed, const nlohmann::json& saved, Draft& draft);
void Cancel(Draft& draft);
std::pair<RE::BGSHeadPart*, std::uint64_t> ThumbnailPart(RE::Actor* actor, RE::FormID id);
bool RestoreExternal(RE::Actor* actor, const nlohmann::json& saved);
RE::BGSColorForm* SavedColor(const nlohmann::json& saved);
RE::BGSHeadPart* SavedHair(const nlohmann::json& saved);
bool CanRestoreHair(RE::Actor* actor, RE::BGSHeadPart* part);
RE::BGSHeadPart* VisualHair(RE::Actor* actor, const nlohmann::json& saved);
bool HairAwaitingBones(RE::Actor* actor, RE::BGSHeadPart* hair);
}
