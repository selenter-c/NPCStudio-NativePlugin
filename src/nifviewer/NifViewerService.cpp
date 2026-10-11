#include "nifviewer/NifViewerService.h"
#include "nifviewer/NifViewerConfig.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace NPCStudio::NifViewer {

NifViewerService& NifViewerService::GetSingleton() {
    static NifViewerService instance;
    return instance;
}

bool NifViewerService::Initialize() {
    if (m_state != ViewerState::Uninitialized) {
        return true;
    }

    spdlog::info("Initializing NifViewerService...");

    if (!m_renderer.Initialize()) {
        m_lastError = "Failed to initialize renderer";
        m_state = ViewerState::Error;
        spdlog::error(m_lastError);
        return false;
    }

    if (!m_transport.Initialize(m_renderer.GetDevice())) {
        m_lastError = "Failed to initialize frame transport";
        m_state = ViewerState::Error;
        spdlog::error(m_lastError);
        return false;
    }

    m_state = ViewerState::Initialized;
    spdlog::info("NifViewerService initialized successfully");
    return true;
}

void NifViewerService::Shutdown() {
    spdlog::info("Shutting down NifViewerService...");

    UnloadModel();
    m_transport.Shutdown();
    m_renderer.Shutdown();

    m_state = ViewerState::Uninitialized;
    m_visible = true;
    m_dirty = true;
    m_lastError.clear();
    m_currentModelPath.clear();

    spdlog::info("NifViewerService shutdown complete");
}

bool NifViewerService::LoadModel(const std::string& assetPath, uint32_t width, uint32_t height) {
    if (m_state == ViewerState::Uninitialized || m_state == ViewerState::Error) {
        m_lastError = "Service not initialized";
        return false;
    }

    auto& config = GetConfig();
    width = std::clamp(width, config.minWidth, config.maxWidth);
    height = std::clamp(height, config.minHeight, config.maxHeight);

    m_state = ViewerState::Loading;
    m_currentModelPath = assetPath;

    spdlog::info("Loading model: {} ({}x{})", assetPath, width, height);

    m_modelLoader.Unload();

    if (!m_modelLoader.LoadFromFile(assetPath)) {
        m_lastError = m_modelLoader.GetLastError();
        m_state = ViewerState::Error;
        spdlog::error("Failed to load model: {}", m_lastError);
        return false;
    }

    if (!m_renderer.SetModel(m_modelLoader.GetMeshData())) {
        m_lastError = "Failed to upload model to GPU";
        m_state = ViewerState::Error;
        spdlog::error(m_lastError);
        return false;
    }

    m_renderer.GetRenderTarget().Resize(m_renderer.GetDevice(), width, height);

    const auto& mesh = m_modelLoader.GetMeshData();
    m_currentYaw = 45.0f;
    m_currentPitch = -15.0f;
    m_currentDistance = mesh.radius * 2.5f;
    if (m_currentDistance < 0.5f) m_currentDistance = 3.0f;
    
    spdlog::info("Camera set: yaw={}, pitch={}, dist={}, target=({:.2f}, {:.2f}, {:.2f})",
                 m_currentYaw, m_currentPitch, m_currentDistance,
                 mesh.center[0], mesh.center[1], mesh.center[2]);

    m_state = ViewerState::Ready;
    m_dirty = true;

    spdlog::info("Model loaded successfully: {}", assetPath);
    return true;
}

void NifViewerService::UnloadModel() {
    m_renderer.ClearModel();
    m_modelLoader.Unload();
    m_currentModelPath.clear();
    m_dirty = true;

    if (m_state == ViewerState::Ready || m_state == ViewerState::Rendering) {
        m_state = ViewerState::Initialized;
    }
}

void NifViewerService::Rotate(float deltaYaw, float deltaPitch) {
    if (!HasModel()) return;

    m_currentYaw += deltaYaw;
    m_currentPitch += deltaPitch;

    m_renderer.SetCamera(m_currentYaw, m_currentPitch, m_currentDistance);
    m_dirty = true;
}

void NifViewerService::Zoom(float delta) {
    if (!HasModel()) return;

    auto& config = GetConfig();
    m_currentDistance -= delta * config.defaultZoom;
    m_currentDistance = std::clamp(m_currentDistance, config.minZoom, config.maxZoom);

    m_renderer.SetCamera(m_currentYaw, m_currentPitch, m_currentDistance);
    m_dirty = true;
}

void NifViewerService::ResetCamera() {
    if (!HasModel()) return;

    m_renderer.AutoFrameModel(m_modelLoader.GetMeshData());
    
    const auto& mesh = m_modelLoader.GetMeshData();
    m_currentYaw = 45.0f;
    m_currentPitch = -15.0f;
    m_currentDistance = mesh.radius * 2.5f;
    if (m_currentDistance < 0.5f) m_currentDistance = 3.0f;
    
    m_dirty = true;
}

void NifViewerService::SetViewport(uint32_t width, uint32_t height) {
    auto& config = GetConfig();
    width = std::clamp(width, config.minWidth, config.maxWidth);
    height = std::clamp(height, config.minHeight, config.maxHeight);

    if (m_renderer.IsInitialized()) {
        m_renderer.GetRenderTarget().Resize(m_renderer.GetDevice(), width, height);
        m_dirty = true;
    }
}

void NifViewerService::SetVisible(bool visible) {
    m_visible = visible;
    if (visible && HasModel()) {
        m_dirty = true;
    }
}

std::string NifViewerService::RenderFrame() {
    if (!m_visible || !HasModel() || m_state == ViewerState::Uninitialized) {
        spdlog::debug("RenderFrame skipped: visible={}, hasModel={}, state={}", 
                      m_visible, HasModel(), GetStateString());
        return "";
    }

    if (!m_dirty) {
        spdlog::debug("RenderFrame skipped: not dirty");
        return "";
    }

    m_state = ViewerState::Rendering;

    spdlog::info("Rendering frame...");
    if (!m_renderer.Render()) {
        spdlog::warn("Render failed");
        m_state = ViewerState::Ready;
        return "";
    }

    spdlog::info("Capturing frame as base64...");
    std::string base64 = m_transport.CaptureFrameAsBase64(m_renderer.GetRenderTarget().GetColorTexture());
    spdlog::info("Frame captured: {} bytes", base64.size());

    m_dirty = false;
    m_state = ViewerState::Ready;

    return base64;
}

bool NifViewerService::HasModel() const {
    return m_renderer.HasModel() && m_modelLoader.IsLoaded();
}

std::string NifViewerService::GetStateString() const {
    switch (m_state) {
        case ViewerState::Uninitialized: return "uninitialized";
        case ViewerState::Initialized: return "initialized";
        case ViewerState::Loading: return "loading";
        case ViewerState::Ready: return "ready";
        case ViewerState::Rendering: return "rendering";
        case ViewerState::Error: return "error";
        default: return "unknown";
    }
}

}