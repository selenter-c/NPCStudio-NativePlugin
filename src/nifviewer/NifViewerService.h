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
    bool IsInitialized() const { return m_state != ViewerState::Uninitialized && !m_initFailed; }
    std::string GetStateString() const;
    std::string GetLastError() const { return m_lastError; }
    std::string GetLastErrorCode() const { return m_lastErrorCode; }

private:
    // After a non-fatal load error (missing FaceGen, bad mesh), the service stays
    // in Initialized/Ready so subsequent loads are not blocked. Only renderer/transport
    // init failures set m_initFailed and make the service unusable until Shutdown+Initialize.
    bool IsServiceReady() const;

    NifViewerService() = default;

    NifModelLoader m_modelLoader;
    NPCModelAssembler m_npcAssembler;
    NifRenderBackend m_renderer;
    NifFrameTransport m_transport;

    ViewerState m_state = ViewerState::Uninitialized;
    bool m_initFailed = false;
    bool m_visible = true;
    bool m_dirty = true;
    std::string m_lastError;
    std::string m_lastErrorCode;
    std::string m_currentModelPath;
    
    MeshData m_currentMesh;
    bool m_hasCurrentMesh = false;
    
    float m_currentYaw = 0.0f;
    float m_currentPitch = 10.0f;
    float m_currentDistance = 3.0f;
    
    bool m_debugRender = false;
};

}