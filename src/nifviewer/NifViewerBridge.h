#pragma once

#include <string>
#include <functional>

namespace NPCStudio::NifViewer {

class NifViewerService;

class NifViewerBridge {
public:
    static NifViewerBridge& GetSingleton();

    void Initialize(NifViewerService* service);
    void Shutdown();

    void HandleCommand(const std::string& command, const std::string& jsonPayload);
    void SendResponse(const std::string& event, const std::string& jsonData);

    void SetViewHandle(void* view) { m_view = view; }
    void* GetViewHandle() const { return m_view; }

private:
    NifViewerBridge() = default;

    void HandleReady(const std::string& requestId);
    void HandleLoadModel(const std::string& requestId, const std::string& payload);
    void HandleUnloadModel(const std::string& requestId);
    void HandleRotate(const std::string& requestId, const std::string& payload);
    void HandleZoom(const std::string& requestId, const std::string& payload);
    void HandleResetCamera(const std::string& requestId);
    void HandleResize(const std::string& requestId, const std::string& payload);
    void HandleSetVisible(const std::string& requestId, const std::string& payload);
    void HandleGetStatus(const std::string& requestId);
    void HandleGetNPCMeshPath(const std::string& requestId, const std::string& payload);

    void SendError(const std::string& requestId, const std::string& code, const std::string& message);

    NifViewerService* m_service = nullptr;
    void* m_view = nullptr;
};

}