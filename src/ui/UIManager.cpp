#include "ui/UIManager.h"
#include "api/PrismUIBridge.h"
#include "npc/NPCFactory.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

namespace NPCStudio::UI {

using json = nlohmann::json;

static std::string GetSettingsPath() {
    auto path = SKSE::log::log_directory();
    if (!path) return "";
    auto settingsDir = path->parent_path() / "NPCStudio";
    std::filesystem::create_directories(settingsDir);
    return (settingsDir / "settings.json").string();
}

UIManager& UIManager::GetSingleton() {
    static UIManager instance;
    return instance;
}

void UIManager::LoadSettings() {
    auto path = GetSettingsPath();
    if (path.empty()) return;
    
    try {
        if (std::filesystem::exists(path)) {
            std::ifstream file(path);
            if (file.is_open()) {
                json j = json::parse(file);
                m_pauseGame = j.value("pauseGame", false);
                spdlog::info("Settings loaded: pauseGame={}", m_pauseGame);
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to load settings: {}", e.what());
    }
}

void UIManager::SaveSettings() {
    auto path = GetSettingsPath();
    if (path.empty()) return;
    
    try {
        json j;
        j["pauseGame"] = m_pauseGame;
        
        std::ofstream file(path);
        if (file.is_open()) {
            file << j.dump(2);
            spdlog::info("Settings saved: pauseGame={}", m_pauseGame);
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to save settings: {}", e.what());
    }
}

void UIManager::Register() {
    LoadSettings();
    
    auto& bridge = API::PrismUIBridge::GetSingleton();
    if (!bridge.Initialize()) {
        spdlog::error("PrismUI unavailable - UI disabled");
        return;
    }

    m_view = bridge.CreateView("NPCStudio/index.html");
    if (!m_view) {
        spdlog::error("Failed to create PrismUI view");
        return;
    }

    bridge.HideView(m_view);

    bridge.RegisterJSListener(m_view, "closeDashboard", [](const std::string&) -> std::string {
        spdlog::info("closeDashboard called from JS");
        UIManager::GetSingleton().CloseDashboard();
        return "ok";
    });

    bridge.RegisterJSListener(m_view, "uiReady", [this](const std::string&) -> std::string {
        spdlog::info("UI ready");
        auto& bridge = API::PrismUIBridge::GetSingleton();
        json state;
        state["pauseGame"] = m_pauseGame;
        bridge.CallJS(m_view, "setInitialState", state.dump());
        return "ok";
    });

    bridge.RegisterJSListener(m_view, "testButton", [](const std::string&) -> std::string {
        spdlog::info("test");
        return "ok";
    });

    bridge.RegisterJSListener(m_view, "getNPCList", [this](const std::string&) -> std::string {
        auto& factory = NPC::NPCFactory::GetSingleton();
        auto list = factory.GetNPCList();
        
        json j = json::array();
        for (const auto& npc : list) {
            json entry;
            entry["formID"] = npc.formID;
            entry["editorID"] = npc.editorID;
            entry["name"] = npc.name;
            j.push_back(entry);
        }
        
        auto jsonStr = j.dump();
        API::PrismUIBridge::GetSingleton().CallJS(m_view, "setNPCList", jsonStr);
        return "ok";
    });

    bridge.RegisterJSListener(m_view, "spawnNPC", [](const std::string& formIdStr) -> std::string {
        try {
            uint32_t formID = std::stoul(formIdStr, nullptr, 16);
            auto& factory = NPC::NPCFactory::GetSingleton();
            bool success = factory.SpawnNPC(formID);
            return success ? "ok" : "error";
        } catch (...) {
            spdlog::error("Invalid FormID: {}", formIdStr);
            return "error";
        }
    });

    bridge.RegisterJSListener(m_view, "togglePause", [](const std::string& value) -> std::string {
        auto& ui = UIManager::GetSingleton();
        bool enabled = (value == "true" || value == "1");
        ui.SetPauseEnabled(enabled);
        return enabled ? "true" : "false";
    });

    spdlog::info("UIManager registered (dashboard hidden, press SHIFT+8 to open)");
}

void UIManager::OpenDashboard() {
    if (m_isOpen || !m_view) return;
    API::PrismUIBridge::GetSingleton().ShowView(m_view, m_pauseGame);
    m_isOpen = true;
    spdlog::info("Dashboard opened (pause={})", m_pauseGame);
}

void UIManager::CloseDashboard() {
    if (!m_isOpen || !m_view) return;
    API::PrismUIBridge::GetSingleton().HideView(m_view);
    m_isOpen = false;
    spdlog::info("Dashboard closed");
}

void UIManager::ToggleDashboard() {
    if (m_isOpen) CloseDashboard();
    else OpenDashboard();
}

bool UIManager::IsOpen() const {
    return m_isOpen;
}

bool UIManager::IsPauseEnabled() const {
    return m_pauseGame;
}

void UIManager::SetPauseEnabled(bool enabled) {
    m_pauseGame = enabled;
    SaveSettings();
    
    if (m_isOpen && m_view) {
        auto& bridge = API::PrismUIBridge::GetSingleton();
        bridge.HideView(m_view);
        bridge.ShowView(m_view, m_pauseGame);
    }
    
    spdlog::info("Pause game set to: {}", enabled);
}

} // namespace NPCStudio::UI