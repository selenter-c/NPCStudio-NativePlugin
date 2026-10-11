#pragma once

#include "NifModelLoader.h"
#include "NifRenderBackend.h"
#include "NifFrameTransport.h"
#include "NPCModelAssembler.h"
#include <string>
#include <cstdint>

namespace NPCStudio::NifViewer {

enum class ViewerState {
    Uninitialized,
    Initialized,
    Loading,
    Ready,
    Rendering,
    Error
};

class NifViewerService {
public:
    static NifViewerService& GetSingleton();

    bool Initialize();
    void Shutdown();

    bool LoadModel(const std::string& assetPath, uint32_t width = 512, uint32_t height = 512);
    bool LoadNPC(uint32_t formID, uint32_t width = 512, uint32_t height = 512);
    void UnloadModel();

    void Rotate(float deltaYaw, float deltaPitch);
    void Zoom(float delta);
    void ResetCamera();

    void SetViewport(uint32_t width, uint32_t height);
    void SetVisible(bool visible);

    std::string RenderFrame();

    bool HasModel() const;
    bool IsInitialized() const { return m_state != ViewerState::Uninitialized; }
    std::string GetStateString() const;
    std::string GetLastError() const { return m_lastError; }

private:
    NifViewerService() = default;

    NifModelLoader m_modelLoader;
    NPCModelAssembler m_npcAssembler;
    NifRenderBackend m_renderer;
    NifFrameTransport m_transport;

    ViewerState m_state = ViewerState::Uninitialized;
    bool m_visible = true;
    bool m_dirty = true;
    std::string m_lastError;
    std::string m_currentModelPath;
    
    float m_currentYaw = 45.0f;
    float m_currentPitch = -15.0f;
    float m_currentDistance = 3.0f;
};

}