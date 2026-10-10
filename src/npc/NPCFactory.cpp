#include "npc/NPCFactory.h"
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESNPC.h>
#include <RE/T/TESForm.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESObjectREFR.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESWorldSpace.h>
#include <RE/A/Actor.h>
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
        spdlog::error("SpawnNPC: PlayerCharacter not found");
        return false;
    }

    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) {
        spdlog::error("SpawnNPC: TESDataHandler not available");
        return false;
    }

    auto* base = RE::TESForm::LookupByID<RE::TESNPC>(formID);
    if (!base) {
        spdlog::error("SpawnNPC: TESNPC {:08X} not found", formID);
        return false;
    }

    auto* parentCell = player->GetParentCell();
    auto* worldSpace = player->GetWorldspace();

    if (!parentCell) {
        spdlog::error("SpawnNPC: Player has no parent cell");
        return false;
    }

    const auto pos = player->GetPosition();
    const auto angle = player->GetAngle();

    constexpr float spawnDist = 150.0f;

    RE::NiPoint3 spawnPos{
        pos.x + spawnDist * std::sin(angle.z),
        pos.y + spawnDist * std::cos(angle.z),
        pos.z
    };

    RE::NiPoint3 spawnAngle{
        0.0f,
        0.0f,
        angle.z + 3.14159265f
    };

    spdlog::info(
        "SpawnNPC: base={:08X}, cell={:08X}, worldspace={}, pos=({:.2f}, {:.2f}, {:.2f})",
        base->GetFormID(),
        parentCell->GetFormID(),
        worldSpace ? fmt::format("{:08X}", worldSpace->GetFormID()) : "null",
        spawnPos.x, spawnPos.y, spawnPos.z
    );

    auto handle = dataHandler->CreateReferenceAtLocation(
        base,
        spawnPos,
        spawnAngle,
        parentCell,
        worldSpace,
        nullptr,
        nullptr,
        RE::ObjectRefHandle(),
        true,
        true
    );

    auto ref = handle.get();

    if (!ref) {
        spdlog::error("SpawnNPC: CreateReferenceAtLocation failed for base {:08X}", formID);
        return false;
    }

    auto* actor = ref->As<RE::Actor>();

    if (!actor) {
        spdlog::error(
            "SpawnNPC: Reference {:08X} is not an Actor; base={:08X}",
            ref->GetFormID(), formID
        );
        return false;
    }

    actor->Enable(false);

    const char* name = base->GetName();

    spdlog::info(
        "SpawnNPC: created '{}' base={:08X}, reference={:08X}, disabled={}, is3DLoaded={}, get3D={}",
        name ? name : "Unknown",
        base->GetFormID(),
        actor->GetFormID(),
        actor->IsDisabled(),
        actor->Is3DLoaded(),
        actor->Get3D() != nullptr
    );

    return true;
}

} // namespace NPCStudio::NPC