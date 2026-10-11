#pragma once

#include <cstdint>
#include <string>

namespace NPCStudio::NifViewer {

struct ViewerConfig {
    uint32_t defaultWidth = 512;
    uint32_t defaultHeight = 512;
    uint32_t maxWidth = 1024;
    uint32_t maxHeight = 1024;
    uint32_t minWidth = 64;
    uint32_t minHeight = 64;
    
    float defaultFOV = 45.0f;
    float minZoom = 0.5f;
    float maxZoom = 5.0f;
    float defaultZoom = 1.0f;
    
    float pitchLimit = 89.0f;
    
    bool enableTransparency = true;
    
    std::string resourceRoot = "meshes";
};

inline ViewerConfig& GetConfig() {
    static ViewerConfig config;
    return config;
}

}