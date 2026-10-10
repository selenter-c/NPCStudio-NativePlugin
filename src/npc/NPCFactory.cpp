#include "npc/NPCFactory.h"
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESNPC.h>
#include <RE/T/TESForm.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESObjectREFR.h>
#include <RE/N/NiPoint3.h>
#include <spdlog/spdlog.h>
#include <cmath>

namespace NPCStudio::NPC {

NPCFactory& NPCFactory::GetSingleton() {
    static NPCFactory instance;
    return instance;
}

std::vector<NPCEntry> NPCFactory::GetNPCList() const {
    std::vector<NPCEntry> result;
    
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) {
        spdlog::error("TESDataHandler not available");
        return result;
    }

    auto& npcArray = dataHandler->GetFormArray<RE::TESNPC>();
    result.reserve(npcArray.size());

    for (auto* form : npcArray) {
        if (!form) continue;
        
        auto* npc = form->As<RE::TESNPC>();
        if (!npc) continue;
        
        NPCEntry entry;
        entry.formID = npc->GetFormID();
        
        const char* edid = npc->GetFormEditorID();
        entry.editorID = edid ? edid : "";
        
        const char* fullName = npc->GetName();
        entry.name = fullName ? fullName : entry.editorID;
        
        if (entry.name.empty() && entry.editorID.empty()) {
            entry.name = "Unknown NPC";
        }
        
        result.push_back(std::move(entry));
    }

    spdlog::info("Found {} NPCs in loaded plugins", result.size());
    return result;
}

bool NPCFactory::SpawnNPC(uint32_t formID) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        spdlog::error("Player not found");
        return false;
    }

    auto* base = RE::TESForm::LookupByID<RE::TESNPC>(formID);
    if (!base) {
        spdlog::error("NPC with FormID {:08X} not found", formID);
        return false;
    }

    auto pos = player->GetPosition();
    auto angle = player->GetAngle();
    
    float spawnDist = 150.0f;
    RE::NiPoint3 spawnPos;
    spawnPos.x = pos.x + spawnDist * std::sin(angle.z);
    spawnPos.y = pos.y + spawnDist * std::cos(angle.z);
    spawnPos.z = pos.z;

    RE::NiPoint3 spawnRot;
    spawnRot.x = 0.0f;
    spawnRot.y = 0.0f;
    spawnRot.z = angle.z + 3.14159f;

    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) {
        spdlog::error("TESDataHandler not available for spawning");
        return false;
    }

    auto handle = dataHandler->CreateReferenceAtLocation(
        base,
        spawnPos,
        spawnRot,
        player->GetParentCell(),
        nullptr,
        nullptr,
        nullptr,
        RE::ObjectRefHandle(),
        true,
        false
    );

    if (!handle.get()) {
        spdlog::error("CreateReferenceAtLocation failed for NPC {:08X}", formID);
        return false;
    }

    const char* name = base->GetName();
    spdlog::info("Spawned NPC '{}' ({:08X}) at player position", 
                 name ? name : "unknown", formID);
    return true;
}

} // namespace NPCStudio::NPC