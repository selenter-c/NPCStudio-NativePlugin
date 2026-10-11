#include "nifviewer/NifViewerBridge.h"
#include "nifviewer/NifViewerService.h"
#include "api/PrismUIBridge.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <RE/Skyrim.h>

namespace NPCStudio::NifViewer {

using json = nlohmann::json;

NifViewerBridge& NifViewerBridge::GetSingleton() {
    static NifViewerBridge instance;
    return instance;
}

void NifViewerBridge::Initialize(NifViewerService* service) {
    m_service = service;
    spdlog::info("NifViewerBridge initialized");
}

void NifViewerBridge::Shutdown() {
    m_service = nullptr;
    m_view = nullptr;
}

void NifViewerBridge::HandleCommand(const std::string& command, const std::string& jsonPayload) {
    if (!m_service) {
        spdlog::warn("NifViewerBridge: no service, ignoring command '{}'", command);
        return;
    }

    std::string requestId;
    try {
        if (!jsonPayload.empty()) {
            auto parsed = json::parse(jsonPayload, nullptr, false);
            if (parsed.is_object() && parsed.contains("requestId")) {
                requestId = parsed["requestId"].dump();
            }
        }
    } catch (...) {}

    spdlog::debug("NifViewer command: {}", command);

    if (command == "ready") {
        HandleReady(requestId);
    } else if (command == "loadModel") {
        HandleLoadModel(requestId, jsonPayload);
    } else if (command == "unloadModel") {
        HandleUnloadModel(requestId);
    } else if (command == "rotate") {
        HandleRotate(requestId, jsonPayload);
    } else if (command == "zoom") {
        HandleZoom(requestId, jsonPayload);
    } else if (command == "resetCamera") {
        HandleResetCamera(requestId);
    } else if (command == "resize") {
        HandleResize(requestId, jsonPayload);
    } else if (command == "setVisible") {
        HandleSetVisible(requestId, jsonPayload);
    } else if (command == "getStatus") {
        HandleGetStatus(requestId);
    } else if (command == "getNPCMeshPath") {
        HandleGetNPCMeshPath(requestId, jsonPayload);
    } else if (command == "loadNPC") {
        HandleLoadNPC(requestId, jsonPayload);
    } else {
        SendError(requestId, "UNKNOWN_COMMAND", "Unknown command: " + command);
    }
}

void NifViewerBridge::SendResponse(const std::string& event, const std::string& jsonData) {
    if (!m_view) return;

    try {
        json wrapper;
        wrapper["event"] = event;
        wrapper["data"] = json::parse(jsonData, nullptr, false);

        API::PrismUIBridge::GetSingleton().CallJS(m_view, "onNifViewerEvent", wrapper.dump());
    } catch (const std::exception& e) {
        spdlog::error("Failed to send response: {}", e.what());
    }
}

void NifViewerBridge::SendError(const std::string& requestId, const std::string& code, const std::string& message) {
    json response;
    response["event"] = "error";
    response["requestId"] = requestId;
    response["code"] = code;
    response["message"] = message;

    if (m_view) {
        API::PrismUIBridge::GetSingleton().CallJS(m_view, "onNifViewerEvent", response.dump());
    }
    spdlog::warn("NifViewer error [{}]: {}", code, message);
}

void NifViewerBridge::HandleReady(const std::string& requestId) {
    json response;
    response["event"] = "ready";
    response["requestId"] = requestId;
    response["success"] = true;
    response["features"] = {"loadModel", "rotate", "zoom", "resetCamera", "resize"};
    response["state"] = m_service->GetStateString();

    SendResponse("ready", response.dump());
}

void NifViewerBridge::HandleLoadModel(const std::string& requestId, const std::string& payload) {
    try {
        auto j = json::parse(payload);
        std::string assetPath = j.value("assetPath", "");

        if (assetPath.empty()) {
            SendError(requestId, "INVALID_PATH", "Asset path is required");
            return;
        }

        uint32_t width = j.value("width", 512u);
        uint32_t height = j.value("height", 512u);

        bool success = m_service->LoadModel(assetPath, width, height);

        json response;
        response["event"] = "modelLoaded";
        response["requestId"] = requestId;
        response["success"] = success;

        if (success) {
            response["modelId"] = assetPath;
            response["width"] = width;
            response["height"] = height;

            spdlog::info("Rendering frame after model load...");
            std::string frameData = m_service->RenderFrame();
            spdlog::info("Frame rendered: {} bytes base64", frameData.size());
            
            if (!frameData.empty()) {
                response["frameData"] = frameData;
                spdlog::info("Frame data added to response (total JSON size: {} bytes)", response.dump().size());
            } else {
                spdlog::warn("RenderFrame returned empty data!");
            }
        } else {
            response["error"] = m_service->GetLastError();
        }

        std::string jsonStr = response.dump();
        spdlog::info("Sending modelLoaded response: {} bytes", jsonStr.size());
        SendResponse("modelLoaded", jsonStr);

    } catch (const std::exception& e) {
        SendError(requestId, "PARSE_ERROR", std::string("Invalid payload: ") + e.what());
    }
}

void NifViewerBridge::HandleUnloadModel(const std::string& requestId) {
    m_service->UnloadModel();

    json response;
    response["event"] = "modelUnloaded";
    response["requestId"] = requestId;
    response["success"] = true;

    SendResponse("modelUnloaded", response.dump());
}

void NifViewerBridge::HandleRotate(const std::string& requestId, const std::string& payload) {
    try {
        auto j = json::parse(payload);
        float deltaYaw = j.value("deltaYaw", 0.0f);
        float deltaPitch = j.value("deltaPitch", 0.0f);

        m_service->Rotate(deltaYaw, deltaPitch);

        std::string frameData = m_service->RenderFrame();

        json response;
        response["event"] = "frameUpdated";
        response["requestId"] = requestId;
        response["success"] = true;
        if (!frameData.empty()) {
            response["frameData"] = frameData;
        }

        SendResponse("frameUpdated", response.dump());

    } catch (const std::exception& e) {
        SendError(requestId, "PARSE_ERROR", std::string("Invalid rotate payload: ") + e.what());
    }
}

void NifViewerBridge::HandleZoom(const std::string& requestId, const std::string& payload) {
    try {
        auto j = json::parse(payload);
        float delta = j.value("delta", 0.0f);

        m_service->Zoom(delta);

        std::string frameData = m_service->RenderFrame();

        json response;
        response["event"] = "frameUpdated";
        response["requestId"] = requestId;
        response["success"] = true;
        if (!frameData.empty()) {
            response["frameData"] = frameData;
        }

        SendResponse("frameUpdated", response.dump());

    } catch (const std::exception& e) {
        SendError(requestId, "PARSE_ERROR", std::string("Invalid zoom payload: ") + e.what());
    }
}

void NifViewerBridge::HandleResetCamera(const std::string& requestId) {
    m_service->ResetCamera();

    std::string frameData = m_service->RenderFrame();

    json response;
    response["event"] = "frameUpdated";
    response["requestId"] = requestId;
    response["success"] = true;
    if (!frameData.empty()) {
        response["frameData"] = frameData;
    }

    SendResponse("frameUpdated", response.dump());
}

void NifViewerBridge::HandleResize(const std::string& requestId, const std::string& payload) {
    try {
        auto j = json::parse(payload);
        uint32_t width = j.value("width", 512u);
        uint32_t height = j.value("height", 512u);

        m_service->SetViewport(width, height);

        std::string frameData = m_service->RenderFrame();

        json response;
        response["event"] = "resized";
        response["requestId"] = requestId;
        response["success"] = true;
        response["width"] = width;
        response["height"] = height;
        if (!frameData.empty()) {
            response["frameData"] = frameData;
        }

        SendResponse("resized", response.dump());

    } catch (const std::exception& e) {
        SendError(requestId, "PARSE_ERROR", std::string("Invalid resize payload: ") + e.what());
    }
}

void NifViewerBridge::HandleSetVisible(const std::string& requestId, const std::string& payload) {
    try {
        auto j = json::parse(payload);
        bool visible = j.value("visible", true);

        m_service->SetVisible(visible);

        json response;
        response["event"] = "visibilityChanged";
        response["requestId"] = requestId;
        response["success"] = true;
        response["visible"] = visible;

        SendResponse("visibilityChanged", response.dump());

    } catch (const std::exception& e) {
        SendError(requestId, "PARSE_ERROR", std::string("Invalid visibility payload: ") + e.what());
    }
}

void NifViewerBridge::HandleGetStatus(const std::string& requestId) {
    json response;
    response["event"] = "status";
    response["requestId"] = requestId;
    response["state"] = m_service->GetStateString();
    response["hasModel"] = m_service->HasModel();
    response["isInitialized"] = m_service->IsInitialized();

    SendResponse("status", response.dump());
}

static std::string GetPluginFilenameWithExt(RE::TESFile* file) {
    if (!file) return "Skyrim.esm";
    return std::string(file->GetFilename());
}

void NifViewerBridge::HandleLoadNPC(const std::string& requestId, const std::string& payload) {
    try {
        auto j = json::parse(payload);
        uint32_t formID = j.value("formID", 0u);

        if (formID == 0) {
            SendError(requestId, "INVALID_FORMID", "FormID is required");
            return;
        }

        uint32_t width = j.value("width", 512u);
        uint32_t height = j.value("height", 512u);

        spdlog::info("[NPC] LoadNPC command: formID={:08X}, {}x{}", formID, width, height);

        bool success = m_service->LoadNPC(formID, width, height);

        json response;
        response["event"] = "npcLoaded";
        response["requestId"] = requestId;
        response["success"] = success;
        response["formID"] = formID;

        if (success) {
            std::string frameData = m_service->RenderFrame();
            if (!frameData.empty()) {
                response["frameData"] = frameData;
                spdlog::info("[NPC] Frame rendered after NPC load: {} bytes", frameData.size());
            }
        } else {
            response["error"] = m_service->GetLastError();
        }

        SendResponse("npcLoaded", response.dump());

    } catch (const std::exception& e) {
        SendError(requestId, "PARSE_ERROR", std::string("Invalid loadNPC payload: ") + e.what());
    }
}

void NifViewerBridge::HandleGetNPCMeshPath(const std::string& requestId, const std::string& payload) {
    try {
        auto j = json::parse(payload);
        uint32_t formID = j.value("formID", 0u);

        if (formID == 0) {
            SendError(requestId, "INVALID_FORMID", "FormID is required");
            return;
        }

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            SendError(requestId, "NO_DATA_HANDLER", "TESDataHandler not available");
            return;
        }

        auto* form = RE::TESForm::LookupByID(formID);
        if (!form) {
            SendError(requestId, "NPC_NOT_FOUND", "Form not found for FormID: " + std::to_string(formID));
            return;
        }

        auto* npc = form->As<RE::TESNPC>();
        if (!npc) {
            SendError(requestId, "NOT_AN_NPC", "Form is not an NPC");
            return;
        }

        // Use the ORIGINAL NPC's defining file, NOT GetRootFaceNPC()
        auto* definingFile = npc->GetFile();
        std::string pluginFilename = GetPluginFilenameWithExt(definingFile);
        uint32_t localFormID = formID & 0x00FFFFFF;
        
        char faceGenPath[512];
        snprintf(faceGenPath, sizeof(faceGenPath),
                 "meshes\\actors\\character\\facegendata\\facegeom\\%s\\%08X.nif",
                 pluginFilename.c_str(),
                 static_cast<unsigned int>(localFormID));

        std::string meshPath = faceGenPath;
        spdlog::info("[NPC] Resolved FaceGen path: {} (formID={:08X}, plugin={}, localID={:08X})", 
                     meshPath, formID, pluginFilename, localFormID);

        json response;
        response["event"] = "npcMeshPath";
        response["requestId"] = requestId;
        response["success"] = true;
        response["meshPath"] = meshPath;
        response["formID"] = formID;
        response["pluginName"] = pluginFilename;
        response["localFormID"] = localFormID;

        SendResponse("npcMeshPath", response.dump());

    } catch (const std::exception& e) {
        SendError(requestId, "PARSE_ERROR", std::string("Invalid getNPCMeshPath payload: ") + e.what());
    }
}

}
