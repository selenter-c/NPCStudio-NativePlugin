// TODO: Stage 9 — реализовать присвоение имён через NND (NPCs Names Distributor)
// REF: см. SPID/NND документацию для работы с именами
#include "naming/NameDistributor.h"

namespace NPCStudio::Naming {

bool NameDistributor::AssignName(RE::Actor* actor, const std::string& name) {
    // TODO: Stage 9 — установка имени через SetActorFullName или NND API
    return false;
}

std::string NameDistributor::GenerateName(const std::string& race, const std::string& gender) {
    // TODO: Stage 9 — генерация имени через NND конфигурацию
    return "";
}

} // namespace NPCStudio::Naming