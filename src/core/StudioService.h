#pragma once
#include <nlohmann/json.hpp>
#include "core/StateCodec.h"
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
private:
    struct Slot {
        unsigned id = 0;
        bool female = false;
        RE::TESNPC* base = nullptr;
        RE::Actor* actor = nullptr;
        std::string defaultName;
    };
    std::vector<Slot> slots;
    RE::TESObjectREFR* holdingAnchor = nullptr;
    nlohmann::json assignments = nlohmann::json::object();
    std::set<unsigned> usedSlots;
    bool legacyHistoryUnknown = false;
    struct RestoreJob { std::string identity; std::string preset; unsigned attempts = 0; unsigned stage = 0; };
    std::map<unsigned, RestoreJob> restoring;
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
