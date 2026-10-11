#pragma once

#include "NifModelLoader.h"
#include <string>
#include <vector>
#include <cstdint>

namespace RE {
    class TESNPC;
    class TESRace;
    class BGSHeadPart;
    class TESObjectARMO;
}

namespace NPCStudio::NifViewer {

struct SubMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::string texturePath;
    std::string name;
    
    float transform[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
};

struct MultiMeshData {
    std::vector<SubMesh> subMeshes;
    
    float boundsMin[3] = { 0, 0, 0 };
    float boundsMax[3] = { 0, 0, 0 };
    float center[3] = { 0, 0, 0 };
    float radius = 1.0f;
};

struct NPCAssemblyResult {
    bool success = false;
    std::string errorMessage;
    std::string errorCode;
    MultiMeshData meshData;
    
    std::string faceGenPath;
    std::string bodyPath;
    std::vector<std::string> headPartPaths;
    std::vector<std::string> armorPaths;
    
    uint32_t formID = 0;
    std::string pluginName;
    std::string raceName;
    std::string sexName;
    bool hasFaceGen = true;
    bool isCreature = false;
};

class NPCModelAssembler {
public:
    NPCModelAssembler() = default;
    ~NPCModelAssembler() = default;

    NPCAssemblyResult AssembleNPC(uint32_t formID);
    
    static std::vector<std::string> GetFaceGenCandidates(RE::TESNPC* npc, uint32_t originalFormID);
    static std::string GetBodyPath(RE::TESNPC* npc);
    static std::vector<std::string> GetHeadPartPaths(RE::TESNPC* npc);
    static std::vector<std::string> GetArmorPaths(RE::TESNPC* npc);
    static bool IsHumanoidWithFaceGen(RE::TESNPC* npc);

private:
    bool LoadAndMergeMesh(const std::string& path, const std::string& name, 
                          MultiMeshData& result, const float* transform = nullptr);
    void CalculateBounds(MultiMeshData& data);
    
    static std::string GetPluginFolderName(RE::TESFile* file);
    static uint32_t GetLocalFormIDForFaceGen(RE::TESForm* form);
};

}