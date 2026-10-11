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

    std::string fullPath = ResolveGameDataPath(normalizedPath);
    spdlog::info("Loading NIF: {} -> {}", normalizedPath, fullPath);

    try {
        nifly::NifFile nif;
        int result = -1;

        if (std::filesystem::exists(fullPath)) {
            result = nif.Load(fullPath);
            spdlog::info("Loaded from loose file: {}", fullPath);
        } else {
            spdlog::info("File not found on disk, trying BSResource: {}", normalizedPath);
            
            RE::BSResourceNiBinaryStream bsStream(normalizedPath.c_str());
            if (bsStream.good() && bsStream.lastError == RE::BSResource::ErrorCode::kNone) {
                std::vector<uint8_t> buffer;
                char chunk[4096];
                
                while (bsStream.good()) {
                    bsStream.read(chunk, sizeof(chunk));
                    auto bytesRead = bsStream.tell();
                    if (bytesRead > 0 && static_cast<size_t>(bytesRead) <= sizeof(chunk)) {
                        buffer.insert(buffer.end(), chunk, chunk + bytesRead);
                    }
                    if (static_cast<size_t>(bytesRead) < sizeof(chunk)) {
                        break;
                    }
                }
                
                if (!buffer.empty()) {
                    std::istringstream iss(std::string(buffer.begin(), buffer.end()));
                    result = nif.Load(iss);
                    spdlog::info("Loaded from BSA/resource: {} ({} bytes)", normalizedPath, buffer.size());
                } else {
                    m_lastError = "BSResource returned empty data for: " + normalizedPath;
                    spdlog::error(m_lastError);
                    return false;
                }
            } else {
                m_lastError = "File not found and BSResource failed: " + normalizedPath;
                spdlog::error(m_lastError);
                return false;
            }
        }

        if (result != 0) {
            m_lastError = "Failed to parse NIF: " + normalizedPath + " (code " + std::to_string(result) + ")";
            spdlog::error(m_lastError);
            return false;
        }

        if (!ExtractGeometry(nif)) {
            if (m_lastError.empty()) {
                m_lastError = "Failed to extract geometry from NIF";
            }
            return false;
        }

        CalculateBounds();
        m_loaded = true;

        spdlog::info("NIF loaded: {} vertices, {} indices",
                     m_meshData.vertices.size(), m_meshData.indices.size());
        return true;

    } catch (const std::exception& e) {
        m_lastError = std::string("Exception loading NIF: ") + e.what();
        spdlog::error(m_lastError);
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

    spdlog::info("Model bounds: center=({:.2f}, {:.2f}, {:.2f}), radius={:.2f}",
                 m_meshData.center[0], m_meshData.center[1], m_meshData.center[2],
                 m_meshData.radius);
}

}