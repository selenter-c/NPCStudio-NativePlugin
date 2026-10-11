#include "nifviewer/NifViewerService.h"
#include "nifviewer/NifViewerConfig.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace NPCStudio::NifViewer {

NifViewerService& NifViewerService::GetSingleton() {
    static NifViewerService instance;
    return instance;
}

bool NifViewerService::IsServiceReady() const {
    if (m_initFailed) return false;
    if (m_state == ViewerState::Uninitialized || m_state == ViewerState::Error) return false;
    return true;
}

bool NifViewerService::Initialize() {
    if (m_state == ViewerState::Initialized || m_state == ViewerState::Ready ||
        m_state == ViewerState::Loading || m_state == ViewerState::Rendering) {
        return true;
    }

    if (m_initFailed && m_state != ViewerState::Uninitialized) {
        spdlog::error("[NIF] Initialize refused: previous init failure, state={}", GetStateString());
        return false;
    }

    spdlog::info("Initializing NifViewerService...");

    if (!m_renderer.Initialize()) {
        m_lastError = "Failed to initialize renderer";
        m_state = ViewerState::Error;
        m_initFailed = true;
        spdlog::error(m_lastError);
        return false;
    }

    if (!m_transport.Initialize(m_renderer.GetDevice())) {
        m_lastError = "Failed to initialize frame transport";
        m_state = ViewerState::Error;
        m_initFailed = true;
        spdlog::error(m_lastError);
        return false;
    }

    m_state = ViewerState::Initialized;
    m_initFailed = false;
    spdlog::info("NifViewerService initialized successfully");
    return true;
}

void NifViewerService::Shutdown() {
    spdlog::info("Shutting down NifViewerService...");

    UnloadModel();
    m_transport.Shutdown();
    m_renderer.Shutdown();

    m_state = ViewerState::Uninitialized;
    m_initFailed = false;
    m_visible = true;
    m_dirty = true;
    m_lastError.clear();
    m_lastErrorCode.clear();
    m_currentModelPath.clear();

    spdlog::info("NifViewerService shutdown complete");
}

bool NifViewerService::LoadModel(const std::string& assetPath, uint32_t width, uint32_t height) {
    spdlog::info("[NIF] LoadModel entered, state={}, path={}", GetStateString(), assetPath);

    if (!IsServiceReady()) {
        m_lastError = "Service not ready (state=" + GetStateString() + ", initFailed=" + std::to_string(m_initFailed) + ")";
        spdlog::error("[NIF] LoadModel refused: {}", m_lastError);
        return false;
    }

    auto& config = GetConfig();
    width = std::clamp(width, config.minWidth, config.maxWidth);
    height = std::clamp(height, config.minHeight, config.maxHeight);

    m_state = ViewerState::Loading;
    m_currentModelPath = assetPath;
    m_lastError.clear();
    m_lastErrorCode.clear();

    spdlog::info("Loading model: {} ({}x{})", assetPath, width, height);

    m_modelLoader.Unload();

    if (!m_modelLoader.LoadFromFile(assetPath)) {
        m_lastError = m_modelLoader.GetLastError();
        m_state = ViewerState::Initialized;
        spdlog::error("[NIF] Failed to load model '{}': {}", assetPath, m_lastError);
        return false;
    }

    if (!m_renderer.SetModel(m_modelLoader.GetMeshData())) {
        m_lastError = "Failed to upload model to GPU";
        m_state = ViewerState::Initialized;
        spdlog::error("[NIF] {}", m_lastError);
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

bool NifViewerService::LoadNPC(uint32_t formID, uint32_t width, uint32_t height) {
    spdlog::info("[NPC] LoadNPC entered, state={}, formID={:08X}", GetStateString(), formID);

    if (!IsServiceReady()) {
        m_lastError = "Service not ready (state=" + GetStateString() + ", initFailed=" + std::to_string(m_initFailed) + ")";
        spdlog::error("[NPC] LoadNPC refused: {}", m_lastError);
        return false;
    }

    auto& config = GetConfig();
    width = std::clamp(width, config.minWidth, config.maxWidth);
    height = std::clamp(height, config.minHeight, config.maxHeight);

    m_state = ViewerState::Loading;
    m_lastError.clear();
    m_lastErrorCode.clear();

    spdlog::info("[NPC] Loading NPC preview for formID={:08X} ({}x{})", formID, width, height);

    // Clear previous model before assembling new one. If assembly fails the service
    // stays in Initialized with no model — UI receives the error and can try another NPC.
    m_modelLoader.Unload();
    m_renderer.ClearModel();

    auto assemblyResult = m_npcAssembler.AssembleNPC(formID);

    if (!assemblyResult.success) {
        m_lastError = assemblyResult.errorMessage;
        m_lastErrorCode = assemblyResult.errorCode;
        m_state = ViewerState::Initialized;
        spdlog::error("[NPC] Assembly failed for formID={:08X}: {} (code={})", formID, m_lastError, m_lastErrorCode);
        return false;
    }

    const auto& multiMesh = assemblyResult.meshData;

    MeshData combinedMesh;
    for (const auto& subMesh : multiMesh.subMeshes) {
        size_t baseVertex = combinedMesh.vertices.size();
        
        for (const auto& v : subMesh.vertices) {
            Vertex tv;
            tv.x = v.x * subMesh.transform[0] + v.y * subMesh.transform[4] + 
                   v.z * subMesh.transform[8] + subMesh.transform[12];
            tv.y = v.x * subMesh.transform[1] + v.y * subMesh.transform[5] + 
                   v.z * subMesh.transform[9] + subMesh.transform[13];
            tv.z = v.x * subMesh.transform[2] + v.y * subMesh.transform[6] + 
                   v.z * subMesh.transform[10] + subMesh.transform[14];
            
            float nx = v.nx * subMesh.transform[0] + v.ny * subMesh.transform[4] + v.nz * subMesh.transform[8];
            float ny = v.nx * subMesh.transform[1] + v.ny * subMesh.transform[5] + v.nz * subMesh.transform[9];
            float nz = v.nx * subMesh.transform[2] + v.ny * subMesh.transform[6] + v.nz * subMesh.transform[10];
            float len = std::sqrt(nx*nx + ny*ny + nz*nz);
            if (len > 0.0001f) { nx /= len; ny /= len; nz /= len; }
            tv.nx = nx;
            tv.ny = ny;
            tv.nz = nz;
            tv.u = v.u;
            tv.v = v.v;
            combinedMesh.vertices.push_back(tv);
        }
        
        // SubMesh indices are LOCAL (relative to that submesh's vertices).
        // Apply baseVertex offset exactly once here during merge.
        for (auto idx : subMesh.indices) {
            if (idx >= subMesh.vertices.size()) {
                spdlog::warn("[NPC] Index out of range in submesh '{}': idx={}, verts={}, skipping triangle vertex",
                             subMesh.name, idx, subMesh.vertices.size());
                continue;
            }
            combinedMesh.indices.push_back(static_cast<uint32_t>(baseVertex) + idx);
        }
    }

    combinedMesh.boundsMin[0] = multiMesh.boundsMin[0];
    combinedMesh.boundsMin[1] = multiMesh.boundsMin[1];
    combinedMesh.boundsMin[2] = multiMesh.boundsMin[2];
    combinedMesh.boundsMax[0] = multiMesh.boundsMax[0];
    combinedMesh.boundsMax[1] = multiMesh.boundsMax[1];
    combinedMesh.boundsMax[2] = multiMesh.boundsMax[2];
    combinedMesh.center[0] = multiMesh.center[0];
    combinedMesh.center[1] = multiMesh.center[1];
    combinedMesh.center[2] = multiMesh.center[2];
    combinedMesh.radius = multiMesh.radius;

    if (!m_renderer.SetModel(combinedMesh)) {
        m_lastError = "Failed to upload assembled model to GPU";
        m_state = ViewerState::Initialized;
        spdlog::error("[NPC] {}", m_lastError);
        return false;
    }

    m_renderer.GetRenderTarget().Resize(m_renderer.GetDevice(), width, height);

    m_currentYaw = 45.0f;
    m_currentPitch = -15.0f;
    m_currentDistance = combinedMesh.radius * 2.5f;
    if (m_currentDistance < 0.5f) m_currentDistance = 3.0f;

    spdlog::info("[NPC] Camera set: yaw={}, pitch={}, dist={}, target=({:.2f}, {:.2f}, {:.2f})",
                 m_currentYaw, m_currentPitch, m_currentDistance,
                 combinedMesh.center[0], combinedMesh.center[1], combinedMesh.center[2]);

    m_state = ViewerState::Ready;
    m_dirty = true;
    m_currentModelPath = "NPC:" + std::to_string(formID);

    spdlog::info("[NPC] Preview loaded: {} sub-meshes, {} vertices, {} indices",
                multiMesh.subMeshes.size(), combinedMesh.vertices.size(), combinedMesh.indices.size());
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