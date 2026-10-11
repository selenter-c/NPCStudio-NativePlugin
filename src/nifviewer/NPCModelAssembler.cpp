#include "nifviewer/NPCModelAssembler.h"
#include <spdlog/spdlog.h>
#include <RE/Skyrim.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <cstring>

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
    return form->GetLocalFormID();
}

bool NPCModelAssembler::IsHumanoidWithFaceGen(RE::TESNPC* npc) {
    if (!npc) return false;
    
    auto* race = npc->GetRace();
    if (!race) return false;
    
    bool hasFaceGenFlag = race->data.flags.any(RE::RACE_DATA::Flag::kFaceGenHead);
    
    bool hasFaceHeadParts = false;
    for (int8_t i = 0; i < npc->numHeadParts && !hasFaceHeadParts; ++i) {
        auto* hp = npc->headParts[i];
        if (hp && hp->type.get() == RE::BGSHeadPart::HeadPartType::kFace) {
            hasFaceHeadParts = true;
        }
    }
    
    return hasFaceGenFlag || hasFaceHeadParts;
}

std::vector<std::string> NPCModelAssembler::GetFaceGenCandidates(RE::TESNPC* npc, uint32_t originalFormID) {
    std::vector<std::string> candidates;
    if (!npc) return candidates;
    
    auto addCandidatesForForm = [&](RE::TESNPC* targetNPC) {
        if (!targetNPC) return;
        
        const auto& sourceFiles = targetNPC->sourceFiles;
        if (!sourceFiles.array || sourceFiles.array->empty()) return;
        
        uint32_t localFormID = GetLocalFormIDForFaceGen(targetNPC);
        
        // Iterate from last override to first (most likely to least likely)
        for (int i = static_cast<int>(sourceFiles.array->size()) - 1; i >= 0; --i) {
            auto* file = (*sourceFiles.array)[i];
            if (!file) continue;
            
            std::string pluginFilename(file->GetFilename());
            if (pluginFilename.empty()) continue;
            
            char path[512];
            snprintf(path, sizeof(path),
                     "meshes\\actors\\character\\facegendata\\facegeom\\%s\\%08X.nif",
                     pluginFilename.c_str(),
                     static_cast<unsigned int>(localFormID));
            
            candidates.push_back(std::string(path));
        }
    };
    
    addCandidatesForForm(npc);
    
    auto* rootFaceNPC = npc->GetRootFaceNPC();
    if (rootFaceNPC && rootFaceNPC != npc) {
        spdlog::info("[NPC] Root face NPC differs (formID={:08X}), adding its candidates", rootFaceNPC->GetFormID());
        addCandidatesForForm(rootFaceNPC);
    }
    
    return candidates;
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
    
    // Store LOCAL indices — baseVertex offset is applied once during merge in LoadNPC.
    subMesh.vertices = meshData.vertices;
    subMesh.indices = meshData.indices;
    
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
        result.errorCode = "FORM_NOT_FOUND";
        spdlog::error("[NPC] {}", result.errorMessage);
        return result;
    }
    
    auto* npc = form->As<RE::TESNPC>();
    if (!npc) {
        result.errorMessage = "Form is not an NPC";
        result.errorCode = "NOT_AN_NPC";
        spdlog::error("[NPC] {}", result.errorMessage);
        return result;
    }
    
    auto* race = npc->GetRace();
    auto sex = npc->GetSex();
    
    result.raceName = race ? std::string(race->GetFormEditorID()) : "Unknown";
    result.sexName = (sex == RE::SEX::kFemale) ? "Female" : "Male";
    
    auto* definingFile = npc->GetFile();
    result.pluginName = GetPluginFolderName(definingFile);
    
    result.isCreature = !IsHumanoidWithFaceGen(npc);
    result.hasFaceGen = !result.isCreature;
    
    spdlog::info("[NPC] Assembling NPC: formID={:08X}, race={}, sex={}, plugin={}, isCreature={}, hasFaceGen={}",
                formID, result.raceName, result.sexName, result.pluginName, result.isCreature, result.hasFaceGen);
    
    if (result.isCreature) {
        result.errorMessage = "Creature/animal: no FaceGen preview available (race=" + result.raceName + ")";
        result.errorCode = "NO_FACEGEN_CREATURE";
        result.success = false;
        spdlog::info("[NPC] {}", result.errorMessage);
        return result;
    }
    
    auto candidates = GetFaceGenCandidates(npc, formID);
    spdlog::info("[NPC] FaceGen candidates ({} total):", candidates.size());
    for (size_t i = 0; i < candidates.size(); ++i) {
        spdlog::info("[NPC]   #{}: {}", i, candidates[i]);
    }
    
    bool faceGenLoaded = false;
    for (size_t i = 0; i < candidates.size(); ++i) {
        spdlog::info("[NPC] Trying FaceGen candidate #{}: {}", i, candidates[i]);
        if (LoadAndMergeMesh(candidates[i], "FaceGen", result.meshData)) {
            result.faceGenPath = candidates[i];
            faceGenLoaded = true;
            spdlog::info("[NPC] FaceGen candidate #{} loaded successfully", i);
            break;
        }
        spdlog::warn("[NPC] FaceGen candidate #{} not found or failed: {}", i, candidates[i]);
    }
    
    if (!faceGenLoaded) {
        std::string triedPaths;
        for (const auto& c : candidates) {
            if (!triedPaths.empty()) triedPaths += ", ";
            triedPaths += c;
        }
        result.errorMessage = "FaceGen not found for any candidate: " + triedPaths;
        result.errorCode = "FACEGEN_NOT_FOUND";
        result.success = false;
        spdlog::error("[NPC] {}", result.errorMessage);
        return result;
    }
    
    result.bodyPath = GetBodyPath(npc);
    if (!result.bodyPath.empty()) {
        spdlog::info("[NPC] Body path: {}", result.bodyPath);
    } else {
        spdlog::warn("[NPC] No body path found for race={}", result.raceName);
    }
    
    result.headPartPaths = GetHeadPartPaths(npc);
    spdlog::info("[NPC] Found {} head parts", result.headPartPaths.size());
    
    if (faceGenLoaded) {
        spdlog::info("[NPC] FaceGen loaded - skipping separate head parts to avoid duplicate geometry");
        for (size_t i = 0; i < result.headPartPaths.size(); ++i) {
            spdlog::info("[NPC]   Skipped head part #{}: {}", i, result.headPartPaths[i]);
        }
    } else {
        spdlog::info("[NPC] No FaceGen - loading head parts as fallback");
        int headPartIndex = 0;
        for (const auto& hpPath : result.headPartPaths) {
            std::string partName = "HeadPart_" + std::to_string(headPartIndex);
            LoadAndMergeMesh(hpPath, partName, result.meshData);
            ++headPartIndex;
        }
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
        result.errorCode = "MESH_PARSE_FAILED";
        spdlog::error("[NPC] {}", result.errorMessage);
    }
    
    return result;
}

}