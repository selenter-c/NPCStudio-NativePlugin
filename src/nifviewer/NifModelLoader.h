#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace nifly {
    class NifFile;
}

namespace NPCStudio::NifViewer {

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::string texturePath;
    
    float boundsMin[3] = { 0, 0, 0 };
    float boundsMax[3] = { 0, 0, 0 };
    float center[3] = { 0, 0, 0 };
    float radius = 1.0f;
};

class NifModelLoader {
public:
    NifModelLoader() = default;
    ~NifModelLoader();

    bool LoadFromFile(const std::string& path);
    void Unload();

    const MeshData& GetMeshData() const { return m_meshData; }
    bool IsLoaded() const { return m_loaded; }
    const std::string& GetLastError() const { return m_lastError; }

private:
    bool ExtractGeometry(nifly::NifFile& nif);
    void CalculateBounds();

    MeshData m_meshData;
    bool m_loaded = false;
    std::string m_lastError;
};

}