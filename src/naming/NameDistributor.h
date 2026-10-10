#pragma once
// TODO: Stage 9 — реализовать присвоение имён через NND (NPCs Names Distributor)
// REF: см. SPID/NND документацию для работы с именами
#include <string>

namespace NPCStudio::Naming {
    class NameDistributor {
    public:
        static bool AssignName(RE::Actor* actor, const std::string& name);
        static std::string GenerateName(const std::string& race, const std::string& gender);
    };
}