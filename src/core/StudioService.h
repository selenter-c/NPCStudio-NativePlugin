#pragma once
#include <nlohmann/json.hpp>

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
    bool ready = false;
    bool loaded = false;
    bool stateValid = true;
    std::string error;
    nlohmann::json Snapshot() const;
    Slot& Find(unsigned id);
    void RestoreNames();
    void Delete(Slot& slot);
};
}
