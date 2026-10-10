#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace NPCStudio::NPC {

struct NPCEntry {
    uint32_t formID;
    std::string editorID;
    std::string name;
    std::string race;
    std::string gender;
    int level;
};

class NPCFactory {
public:
    static NPCFactory& GetSingleton();
    
    std::vector<NPCEntry> GetNPCList() const;
    bool SpawnNPC(uint32_t formID);
    
private:
    NPCFactory() = default;
};

} // namespace NPCStudio::NPC