#include "nifviewer/NifModelLoader.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <limits>
#include <windows.h>
#include <sstream>

#include <NifFile.hpp>
#include <RE/B/BSResourceNiBinaryStream.h>

namespace NPCStudio::NifViewer {

NifModelLoader::~NifModelLoader() {
    Unload();
}

static std::string ResolveGameDataPath(const std::string& relativePath) {
    auto dataPath = RE::GetINISetting("sGameDataPath:General");
    std::string basePath;
    
    if (dataPath && dataPath->GetString() && strlen(dataPath->GetString()) > 0) {
        basePath = dataPath->GetString();
    } else {
        char modulePath[MAX_PATH];
        GetModuleFileNameA(GetModuleHandle(nullptr), modulePath, MAX_PATH);
        std::string exePath(modulePath);
        size_t lastSlash = exePath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            basePath = exePath.substr(0, lastSlash) + "\\Data";
        }
    }
    
    if (basePath.empty()) {
        return relativePath;
    }
    
    std::string fullPath = basePath;
    if (!fullPath.empty() && fullPath.back() != '\\' && fullPath.back() != '/') {
        fullPath += "\\";
    }
    fullPath += relativePath;
    
    return fullPath;
}

static const char* BSResourceErrorCodeName(RE::BSResource::ErrorCode code) {
    switch (code) {
        case RE::BSResource::ErrorCode::kNone: return "kNone";
        case RE::BSResource::ErrorCode::kNotExist: return "kNotExist";
        case RE::BSResource::ErrorCode::kInvalidPath: return "kInvalidPath";
        case RE::BSResource::ErrorCode::kFileError: return "kFileError";
        case RE::BSResource::ErrorCode::kInvalidType: return "kInvalidType";
        case RE::BSResource::ErrorCode::kMemoryError: return "kMemoryError";
        case RE::BSResource::ErrorCode::kBusy: return "kBusy";
        case RE::BSResource::ErrorCode::kInvalidParam: return "kInvalidParam";
        case RE::BSResource::ErrorCode::kUnsupported: return "kUnsupported";
        default: return "unknown";
    }
}

static std::vector<uint8_t> ReadFromBSStream(RE::BSResourceNiBinaryStream& stream, const std::string& pathLabel) {
    std::vector<uint8_t> buffer;
    
    RE::NiBinaryStream::BufferInfo info{};
    stream.get_info(info);
    spdlog::info("[NIF] BSResource '{}' BufferInfo: totalSize={}, bufferAllocSize={}, bufferReadSize={}",
                 pathLabel, info.totalSize, info.bufferAllocSize, info.bufferReadSize);
    
    if (info.totalSize > 0) {
        buffer.resize(info.totalSize);
        uint32_t posBefore = stream.tell();
        bool readOk = stream.read(reinterpret_cast<char*>(buffer.data()), info.totalSize);
        uint32_t posAfter = stream.tell();
        uint32_t actualRead = posAfter - posBefore;
        
        spdlog::info("[NIF] BSResource '{}' bulk read: requested={}, actual={}, ok={}", 
                     pathLabel, info.totalSize, actualRead, readOk);
        
        if (actualRead < info.totalSize) {
            buffer.resize(actualRead);
        }
    } else {
        spdlog::info("[NIF] BSResource '{}' totalSize=0, reading in chunks", pathLabel);
        char chunk[4096];
        while (stream.good()) {
            uint32_t posBefore = stream.tell();
            bool fullRead = stream.read(chunk, sizeof(chunk));
            uint32_t posAfter = stream.tell();
            uint32_t bytesRead = posAfter - posBefore;
            
            if (bytesRead > 0) {
                buffer.insert(buffer.end(), chunk, chunk + bytesRead);
            }
            
            if (!fullRead || bytesRead == 0) {
                break;
            }
        }
        spdlog::info("[NIF] BSResource '{}' chunked read: {} bytes total", pathLabel, buffer.size());
    }
    
    return buffer;
}

static std::vector<uint8_t> ReadBSResourceFully(const std::string& assetPath) {
    std::string pathForward = assetPath;
    std::replace(pathForward.begin(), pathForward.end(), '\\', '/');
    
    spdlog::info("[NIF] Trying BSResource with backslash path: {}", assetPath);
    {
        RE::BSResourceNiBinaryStream bsStream(assetPath.c_str());
        spdlog::info("[NIF] BSResource backslash: good={}, lastError={} ({})",
                     bsStream.good(), static_cast<int>(bsStream.lastError),
                     BSResourceErrorCodeName(bsStream.lastError));
        
        if (bsStream.good() && bsStream.lastError == RE::BSResource::ErrorCode::kNone) {
            return ReadFromBSStream(bsStream, assetPath);
        }
    }
    
    spdlog::warn("[NIF] Backslash path failed, trying forward slash: {}", pathForward);
    {
        RE::BSResourceNiBinaryStream bsStream2(pathForward.c_str());
        spdlog::info("[NIF] BSResource forward: good={}, lastError={} ({})",
                     bsStream2.good(), static_cast<int>(bsStream2.lastError),
                     BSResourceErrorCodeName(bsStream2.lastError));
        
        if (bsStream2.good() && bsStream2.lastError == RE::BSResource::ErrorCode::kNone) {
            return ReadFromBSStream(bsStream2, pathForward);
        }
    }
    
    spdlog::error("[NIF] BSResource failed for both path formats of '{}'", assetPath);
    return {};
}

static bool s_bsResourceSelfTestDone = false;

static void RunBSResourceSelfTest() {
    if (s_bsResourceSelfTestDone) return;
    s_bsResourceSelfTestDone = true;
    
    const char* testPaths[] = {
        "meshes\\actors\\character\\character assets\\malehead.nif",
        "meshes\\actors\\character\\character assets\\femalehead.nif",
        "meshes\\clutter\\rubble\\rubble_01.nif"
    };
    
    spdlog::info("[NIF] === BSResource self-test start ===");
    bool anyOk = false;
    for (const auto* testPath : testPaths) {
        auto buf = ReadBSResourceFully(testPath);
        if (!buf.empty()) {
            spdlog::info("[NIF] BSResource self-test: OK (bytes={}, path={})", buf.size(), testPath);
            anyOk = true;
            break;
        }
        spdlog::warn("[NIF] BSResource self-test: FAILED for '{}'", testPath);
    }
    
    if (!anyOk) {
        spdlog::error("[NIF] BSResource self-test: ALL PATHS FAILED. "
                      "Possible causes: ABI mismatch between plugin and game, "
                      "Address Library version mismatch, calling from wrong thread, "
                      "or game data not fully loaded yet.");
    }
    spdlog::info("[NIF] === BSResource self-test end ===");
}

bool NifModelLoader::LoadFromFile(const std::string& path) {
    Unload();

    if (path.empty()) {
        m_lastError = "Empty path provided";
        return false;
    }

    std::string normalizedPath = path;
    std::replace(normalizedPath.begin(), normalizedPath.end(), '/', '\\');

    if (normalizedPath.find("..") != std::string::npos) {
        m_lastError = "Path traversal not allowed";
        return false;
    }

    RunBSResourceSelfTest();

    std::string fullPath = ResolveGameDataPath(normalizedPath);
    spdlog::info("[NIF] Requested asset: {}", normalizedPath);
    spdlog::info("[NIF] Resolved full path: {}", fullPath);

    try {
        nifly::NifFile nif;
        int result = -1;
        std::string source;

        bool looseExists = std::filesystem::exists(fullPath);
        spdlog::info("[NIF] Loose file exists: {}", looseExists);

        if (looseExists) {
            auto fileSize = std::filesystem::file_size(fullPath);
            spdlog::info("[NIF] Loose file size: {} bytes", fileSize);
            
            if (fileSize == 0) {
                m_lastError = "Loose file is empty: " + fullPath;
                spdlog::error("[NIF] {}", m_lastError);
                return false;
            }
            
            result = nif.Load(fullPath);
            source = "loose";
            spdlog::info("[NIF] Nifly load from loose file result: {}", result);
        } else {
            spdlog::info("[NIF] Trying BSResource for: {}", normalizedPath);
            
            auto buffer = ReadBSResourceFully(normalizedPath);
            
            if (buffer.empty()) {
                m_lastError = "Failed to read resource (both loose and BSResource): " + normalizedPath;
                spdlog::error("[NIF] {}", m_lastError);
                return false;
            }
            
            spdlog::info("[NIF] Resource read: {} bytes from {}", buffer.size(), normalizedPath);
            
            std::string bufferStr(reinterpret_cast<const char*>(buffer.data()), buffer.size());
            std::istringstream iss(std::move(bufferStr));
            result = nif.Load(iss);
            source = "resource";
            spdlog::info("[NIF] Nifly load from resource result: {}", result);
        }

        if (result != 0) {
            m_lastError = "Failed to parse NIF: " + normalizedPath + " (code " + std::to_string(result) + ", source=" + source + ")";
            spdlog::error("[NIF] {}", m_lastError);
            return false;
        }

        spdlog::info("[NIF] NIF parsed successfully from {}", source);

        if (!ExtractGeometry(nif)) {
            if (m_lastError.empty()) {
                m_lastError = "Failed to extract geometry from NIF";
            }
            spdlog::error("[NIF] Geometry extraction failed: {}", m_lastError);
            return false;
        }

        CalculateBounds();
        m_loaded = true;

        spdlog::info("[NIF] Load complete: {} vertices, {} indices, bounds center=({:.2f},{:.2f},{:.2f}), radius={:.2f}",
                     m_meshData.vertices.size(), m_meshData.indices.size(),
                     m_meshData.center[0], m_meshData.center[1], m_meshData.center[2],
                     m_meshData.radius);
        return true;

    } catch (const std::exception& e) {
        m_lastError = std::string("Exception loading NIF: ") + e.what();
        spdlog::error("[NIF] {}", m_lastError);
        return false;
    }
}

void NifModelLoader::Unload() {
    m_meshData.vertices.clear();
    m_meshData.indices.clear();
    m_meshData.texturePath.clear();
    m_loaded = false;
    m_lastError.clear();
}

bool NifModelLoader::ExtractGeometry(nifly::NifFile& nif) {
    auto shapes = nif.GetShapes();

    if (shapes.empty()) {
        m_lastError = "No shapes found in NIF";
        return false;
    }

    spdlog::info("Found {} shapes in NIF", shapes.size());

    for (auto* shape : shapes) {
        if (!shape) continue;

        uint32_t baseVertex = static_cast<uint32_t>(m_meshData.vertices.size());

        auto* bsTriShape = dynamic_cast<nifly::BSTriShape*>(shape);
        if (bsTriShape) {
            auto& rawVerts = bsTriShape->UpdateRawVertices();
            auto& rawNorms = bsTriShape->UpdateRawNormals();
            auto& rawUvs = bsTriShape->UpdateRawUvs();

            // BSDynamicTriShape may have zero vertices from UpdateRawVertices
            // if the actual data is in dynamicData. Check for that case.
            if (rawVerts.empty()) {
                auto* dynTriShape = dynamic_cast<nifly::BSDynamicTriShape*>(shape);
                if (dynTriShape && !dynTriShape->dynamicData.empty()) {
                    spdlog::info("[NIF] BSTriShape '{}' has no raw verts but has dynamicData ({} Vector4 entries), attempting dynamic read",
                                 shape->name.get(), dynTriShape->dynamicData.size());
                    
                    for (size_t i = 0; i < dynTriShape->dynamicData.size(); ++i) {
                        Vertex v{};
                        v.x = dynTriShape->dynamicData[i].x;
                        v.y = dynTriShape->dynamicData[i].y;
                        v.z = dynTriShape->dynamicData[i].z;
                        m_meshData.vertices.push_back(v);
                    }
                    
                    std::vector<nifly::Triangle> tris;
                    bsTriShape->GetTriangles(tris);
                    for (const auto& tri : tris) {
                        m_meshData.indices.push_back(baseVertex + tri.p1);
                        m_meshData.indices.push_back(baseVertex + tri.p2);
                        m_meshData.indices.push_back(baseVertex + tri.p3);
                    }
                    
                    spdlog::info("[NIF] BSDynamicTriShape '{}': {} verts from dynamicData, {} tris",
                                 shape->name.get(), dynTriShape->dynamicData.size(), tris.size());
                    continue;
                }
                
                spdlog::warn("[NIF] BSTriShape '{}' has no vertex data (neither raw nor dynamic)", shape->name.get());
                continue;
            }

            for (size_t i = 0; i < rawVerts.size(); ++i) {
                Vertex v{};
                v.x = rawVerts[i].x;
                v.y = rawVerts[i].y;
                v.z = rawVerts[i].z;

                if (i < rawNorms.size()) {
                    v.nx = rawNorms[i].x;
                    v.ny = rawNorms[i].y;
                    v.nz = rawNorms[i].z;
                } else {
                    v.nx = 0.0f;
                    v.ny = 0.0f;
                    v.nz = 1.0f;
                }

                if (i < rawUvs.size()) {
                    v.u = rawUvs[i].u;
                    v.v = rawUvs[i].v;
                }

                m_meshData.vertices.push_back(v);
            }

            std::vector<nifly::Triangle> tris;
            bsTriShape->GetTriangles(tris);

            for (const auto& tri : tris) {
                m_meshData.indices.push_back(baseVertex + tri.p1);
                m_meshData.indices.push_back(baseVertex + tri.p2);
                m_meshData.indices.push_back(baseVertex + tri.p3);
            }

            spdlog::debug("BSTriShape '{}': {} verts, {} tris",
                          shape->name.get(), rawVerts.size(), tris.size());
            continue;
        }

        auto* geomData = shape->GetGeomData();
        if (!geomData) continue;

        const auto& verts = geomData->vertices;
        const auto& norms = geomData->normals;

        std::vector<nifly::Vector2> uvs;
        if (!geomData->uvSets.empty()) {
            uvs = geomData->uvSets[0];
        }

        for (size_t i = 0; i < verts.size(); ++i) {
            Vertex v{};
            v.x = verts[i].x;
            v.y = verts[i].y;
            v.z = verts[i].z;

            if (i < norms.size()) {
                v.nx = norms[i].x;
                v.ny = norms[i].y;
                v.nz = norms[i].z;
            } else {
                v.nx = 0.0f;
                v.ny = 0.0f;
                v.nz = 1.0f;
            }

            if (i < uvs.size()) {
                v.u = uvs[i].u;
                v.v = uvs[i].v;
            }

            m_meshData.vertices.push_back(v);
        }

        std::vector<nifly::Triangle> tris;
        geomData->GetTriangles(tris);

        for (const auto& tri : tris) {
            m_meshData.indices.push_back(baseVertex + tri.p1);
            m_meshData.indices.push_back(baseVertex + tri.p2);
            m_meshData.indices.push_back(baseVertex + tri.p3);
        }

        spdlog::debug("NiShape '{}': {} verts, {} tris",
                      shape->name.get(), verts.size(), tris.size());
    }

    if (m_meshData.vertices.empty()) {
        m_lastError = "No vertex data extracted";
        return false;
    }

    return true;
}

void NifModelLoader::CalculateBounds() {
    if (m_meshData.vertices.empty()) {
        return;
    }

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    float maxZ = std::numeric_limits<float>::lowest();

    for (const auto& v : m_meshData.vertices) {
        minX = std::min(minX, v.x);
        minY = std::min(minY, v.y);
        minZ = std::min(minZ, v.z);
        maxX = std::max(maxX, v.x);
        maxY = std::max(maxY, v.y);
        maxZ = std::max(maxZ, v.z);
    }

    m_meshData.boundsMin[0] = minX;
    m_meshData.boundsMin[1] = minY;
    m_meshData.boundsMin[2] = minZ;
    m_meshData.boundsMax[0] = maxX;
    m_meshData.boundsMax[1] = maxY;
    m_meshData.boundsMax[2] = maxZ;

    m_meshData.center[0] = (minX + maxX) * 0.5f;
    m_meshData.center[1] = (minY + maxY) * 0.5f;
    m_meshData.center[2] = (minZ + maxZ) * 0.5f;

    float dx = maxX - minX;
    float dy = maxY - minY;
    float dz = maxZ - minZ;
    m_meshData.radius = std::sqrt(dx * dx + dy * dy + dz * dz) * 0.5f;

    if (m_meshData.radius < 0.001f) {
        m_meshData.radius = 1.0f;
    }

    if (std::isnan(m_meshData.radius) || std::isinf(m_meshData.radius)) {
        spdlog::warn("[NIF] Bounds radius is NaN/Inf, resetting to 1.0");
        m_meshData.radius = 1.0f;
    }

    spdlog::info("Model bounds: center=({:.2f}, {:.2f}, {:.2f}), radius={:.2f}",
                 m_meshData.center[0], m_meshData.center[1], m_meshData.center[2],
                 m_meshData.radius);
}

}