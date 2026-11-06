#pragma once
#include <nlohmann/json.hpp>
#include "core/StateCodec.h"
#include "core/AppearanceService.h"
#include <atomic>
#include <thread>
#include <map>

namespace NPCStudio {
class StudioService {
public:
    static StudioService& Get();
    void Initialize();
    void BeforeLoad();
    void AfterLoad(bool success);
    void NewGame();
    nlohmann::json Execute(const nlohmann::json& command);
    void Save(SKSE::SerializationInterface* stream);
    void Load(SKSE::SerializationInterface* stream);
    void Revert();
    void Pump();
    void PickAppearanceTarget();
    void CancelAppearance();
private:
    struct Slot {
        unsigned id = 0;
        bool female = false;
        RE::TESNPC* base = nullptr;
        RE::Actor* actor = nullptr;
        std::string defaultName;
        RE::TESRace* defaultRace = nullptr;
        RE::BGSColorForm* defaultHairColor = nullptr;
    };
    std::vector<Slot> slots;
    RE::TESObjectREFR* holdingAnchor = nullptr;
    nlohmann::json assignments = nlohmann::json::object();
    std::set<unsigned> usedSlots;
    struct ExternalLook {
        RE::FormID base = 0;
        nlohmann::json look;
        const RE::BSFaceGenNiNode* appliedNode = nullptr;
    };
    std::map<RE::FormID, ExternalLook> externalLooks;
    RE::ObjectRefHandle appearanceTarget;
    Appearance::Draft hairDraft;
    std::atomic<bool> externalWork = false;
    struct RestoreJob { std::string identity; std::string preset; unsigned attempts = 0; unsigned stage = 0; bool raceRequested = false; bool appearanceRestored = false; };
    std::map<unsigned, RestoreJob> restoring;
    unsigned settlingTicks = 0;
    std::atomic<bool> workPending = false;
    std::atomic<bool> taskQueued = false;
    std::atomic<unsigned> worldEpoch = 0;
    bool ready = false;
    bool loaded = false;
    bool stateValid = true;
    std::string error;
    nlohmann::json Snapshot() const;
    Slot& Find(unsigned id);
    void RestoreNames();
    void Delete(Slot& slot);
    void QueueRestore(Slot& slot, const std::string& preset = "");
    std::jthread worker;
};
}
