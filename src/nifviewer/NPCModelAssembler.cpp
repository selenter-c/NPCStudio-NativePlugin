#include "nifviewer/NPCModelAssembler.h"
#include <spdlog/spdlog.h>
#include <RE/Skyrim.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace NPCStudio::NifViewer {

std::string NPCModelAssembler::GetPluginFolderName(RE::TESFile* file) {
    if (!file) return "Skyrim.esm";
    
    std::string filename(file->GetFilename());
    
    size_t dotPos = filename.find_last_of('.');
    if (dotPos != std::string::npos) {
        return filename.substr(0, dotPos);
    }
    return filename;
}

uint32_t NPCModelAssembler::GetLocalFormIDForFaceGen(RE::TESForm* form) {
    if (!form) return 0;
    return form->GetFormID() & 0x00FFFFFF;
}

std::string NPCModelAssembler::GetFaceGenPath(RE::TESNPC* npc, uint32_t originalFormID) {
    if (!npc) return "";
    
    // Use the ORIGINAL NPC's defining file, NOT GetRootFaceNPC()
    // GetRootFaceNPC returns the template NPC which may be in a different plugin
    // FaceGen files are stored under the plugin that DEFINES the NPC record
    auto* definingFile = npc->GetFile();
    std::string pluginFilename = definingFile ? std::string(definingFile->GetFilename()) : "Skyrim.esm";
    
    // Use the ORIGINAL formID passed from JS for localFormID
    uint32_t localFormID = originalFormID & 0x00FFFFFF;
    
    spdlog::info("[NPC] FaceGen debug: originalFormID={:08X}, localFormID={:08X}, plugin={}, npc_formID={:08X}", 
                 originalFormID, localFormID, pluginFilename, npc->GetFormID());
    
    char path[512];
    snprintf(path, sizeof(path),
             "meshes\\actors\\character\\facegendata\\facegeom\\%s\\%08X.nif",
             pluginFilename.c_str(),
             static_cast<unsigned int>(localFormID));
    
    return std::string(path);
}

std::string NPCModelAssembler::GetBodyPath(RE::TESNPC* npc) {
    if (!npc) return "";
    
    auto* race = npc->GetRace();
    if (!race) return "";
    
    auto sex = npc->GetSex();
    int sexIndex = (sex == RE::SEX::kFemale) ? 1 : 0;
    
    const char* skeletonModel = race->skeletonModels[sexIndex].GetModel();
    if (skeletonModel && strlen(skeletonModel) > 0) {
        return std::string(skeletonModel);
    }
    
    return "";
}

std::vector<std::string> NPCModelAssembler::GetHeadPartPaths(RE::TESNPC* npc) {
    std::vector<std::string> paths;
    if (!npc) return paths;
    
    for (int8_t i = 0; i < npc->numHeadParts; ++i) {
        auto* headPart = npc->headParts[i];
        if (!headPart) continue;
        
        const char* model = headPart->GetModel();
        if (model && strlen(model) > 0) {
            paths.emplace_back(model);
            spdlog::debug("[NPC] HeadPart[{}]: type={}, model={}", 
                         i, static_cast<int>(headPart->type.get()), model);
        }
    }
    
    return paths;
}

std::vector<std::string> NPCModelAssembler::GetArmorPaths(RE::TESNPC* npc) {
    std::vector<std::string> paths;
    if (!npc) return paths;
    
    auto* outfit = npc->defaultOutfit;
    if (!outfit) return paths;
    
    spdlog::debug("[NPC] Processing default outfit for armor paths");
    
    return paths;
}

bool NPCModelAssembler::LoadAndMergeMesh(const std::string& path, const std::string& name,
                                          MultiMeshData& result, const float* transform) {
    if (path.empty()) {
        spdlog::warn("[NPC] Empty mesh path for '{}'", name);
        return false;
    }
    
    NifModelLoader loader;
    if (!loader.LoadFromFile(path)) {
        spdlog::error("[NPC] Failed to load mesh '{}' from '{}': {}", 
                     name, path, loader.GetLastError());
        return false;
    }
    
    const auto& meshData = loader.GetMeshData();
    if (meshData.vertices.empty() || meshData.indices.empty()) {
        spdlog::warn("[NPC] Mesh '{}' has no geometry", name);
        return false;
    }
    
    SubMesh subMesh;
    subMesh.name = name;
    subMesh.texturePath = meshData.texturePath;
    
    if (transform) {
        std::copy(transform, transform + 16, subMesh.transform);
    }
    
    uint32_t baseVertex = 0;
    if (!result.subMeshes.empty()) {
        for (const auto& existing : result.subMeshes) {
            baseVertex += static_cast<uint32_t>(existing.vertices.size());
        }
    }
    
    subMesh.vertices = meshData.vertices;
    
    subMesh.indices.reserve(meshData.indices.size());
    for (auto idx : meshData.indices) {
        subMesh.indices.push_back(baseVertex + idx);
    }
    
    spdlog::info("[NPC] Merged mesh '{}': {} verts, {} indices", 
                name, subMesh.vertices.size(), subMesh.indices.size());
    
    result.subMeshes.push_back(std::move(subMesh));
    return true;
}

void NPCModelAssembler::CalculateBounds(MultiMeshData& data) {
    if (data.subMeshes.empty()) {
        data.radius = 1.0f;
        return;
    }
    
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    float maxZ = std::numeric_limits<float>::lowest();
    
    for (const auto& subMesh : data.subMeshes) {
        for (const auto& v : subMesh.vertices) {
            float tx = v.x * subMesh.transform[0] + v.y * subMesh.transform[4] + 
                       v.z * subMesh.transform[8] + subMesh.transform[12];
            float ty = v.x * subMesh.transform[1] + v.y * subMesh.transform[5] + 
                       v.z * subMesh.transform[9] + subMesh.transform[13];
            float tz = v.x * subMesh.transform[2] + v.y * subMesh.transform[6] + 
                       v.z * subMesh.transform[10] + subMesh.transform[14];
            
            minX = std::min(minX, tx);
            minY = std::min(minY, ty);
            minZ = std::min(minZ, tz);
            maxX = std::max(maxX, tx);
            maxY = std::max(maxY, ty);
            maxZ = std::max(maxZ, tz);
        }
    }
    
    data.boundsMin[0] = minX;
    data.boundsMin[1] = minY;
    data.boundsMin[2] = minZ;
    data.boundsMax[0] = maxX;
    data.boundsMax[1] = maxY;
    data.boundsMax[2] = maxZ;
    
    data.center[0] = (minX + maxX) * 0.5f;
    data.center[1] = (minY + maxY) * 0.5f;
    data.center[2] = (minZ + maxZ) * 0.5f;
    
    float dx = maxX - minX;
    float dy = maxY - minY;
    float dz = maxZ - minZ;
    data.radius = std::sqrt(dx * dx + dy * dy + dz * dz) * 0.5f;
    
    if (data.radius < 0.001f) {
        data.radius = 1.0f;
    }
    
    spdlog::info("[NPC] Combined bounds: center=({:.2f},{:.2f},{:.2f}), radius={:.2f}",
                data.center[0], data.center[1], data.center[2], data.radius);
}

NPCAssemblyResult NPCModelAssembler::AssembleNPC(uint32_t formID) {
    NPCAssemblyResult result;
    result.formID = formID;
    
    auto* form = RE::TESForm::LookupByID(formID);
    if (!form) {
        result.errorMessage = "Form not found for FormID: " + std::to_string(formID);
        spdlog::error("[NPC] {}", result.errorMessage);
        return result;
    }
    
    auto* npc = form->As<RE::TESNPC>();
    if (!npc) {
        result.errorMessage = "Form is not an NPC";
        spdlog::error("[NPC] {}", result.errorMessage);
        return result;
    }
    
    auto* race = npc->GetRace();
    auto sex = npc->GetSex();
    
    result.raceName = race ? std::string(race->GetFormEditorID()) : "Unknown";
    result.sexName = (sex == RE::SEX::kFemale) ? "Female" : "Male";
    
    auto* definingFile = npc->GetFile();
    result.pluginName = GetPluginFolderName(definingFile);
    
    spdlog::info("[NPC] Assembling NPC: formID={:08X}, race={}, sex={}, plugin={}",
                formID, result.raceName, result.sexName, result.pluginName);
    
    result.faceGenPath = GetFaceGenPath(npc, formID);
    spdlog::info("[NPC] FaceGen path: {}", result.faceGenPath);
    
    bool hasFaceGen = LoadAndMergeMesh(result.faceGenPath, "FaceGen", result.meshData);
    if (!hasFaceGen) {
        result.errorMessage = "Failed to load FaceGen mesh: " + result.faceGenPath;
        spdlog::error("[NPC] {}", result.errorMessage);
        return result;
    }
    
    result.bodyPath = GetBodyPath(npc);
    if (!result.bodyPath.empty()) {
        spdlog::info("[NPC] Body path: {}", result.bodyPath);
        LoadAndMergeMesh(result.bodyPath, "Body", result.meshData);
    } else {
        spdlog::warn("[NPC] No body path found for race={}", result.raceName);
    }
    
    result.headPartPaths = GetHeadPartPaths(npc);
    spdlog::info("[NPC] Found {} head parts", result.headPartPaths.size());
    
    int headPartIndex = 0;
    for (const auto& hpPath : result.headPartPaths) {
        std::string partName = "HeadPart_" + std::to_string(headPartIndex);
        LoadAndMergeMesh(hpPath, partName, result.meshData);
        ++headPartIndex;
    }
    
    result.armorPaths = GetArmorPaths(npc);
    spdlog::info("[NPC] Found {} armor pieces", result.armorPaths.size());
    
    int armorIndex = 0;
    for (const auto& armorPath : result.armorPaths) {
        std::string armorName = "Armor_" + std::to_string(armorIndex);
        LoadAndMergeMesh(armorPath, armorName, result.meshData);
        ++armorIndex;
    }
    
    CalculateBounds(result.meshData);
    
    result.success = !result.meshData.subMeshes.empty();
    
    if (result.success) {
        spdlog::info("[NPC] Assembly complete: {} sub-meshes, {} total vertices, {} total indices",
                    result.meshData.subMeshes.size(),
                    std::accumulate(result.meshData.subMeshes.begin(), result.meshData.subMeshes.end(), 
                                   size_t(0), [](size_t sum, const SubMesh& sm) { return sum + sm.vertices.size(); }),
                    std::accumulate(result.meshData.subMeshes.begin(), result.meshData.subMeshes.end(),
                                   size_t(0), [](size_t sum, const SubMesh& sm) { return sum + sm.indices.size(); }));
    } else {
        result.errorMessage = "No geometry assembled";
        spdlog::error("[NPC] {}", result.errorMessage);
    }
    
    return result;
}

}