#pragma once
#include <string>

namespace NPCStudio::UI {
    class UIManager {
    public:
        static UIManager& GetSingleton();
        void Register();
        void OpenDashboard();
        void CloseDashboard();
        void ToggleDashboard();
        bool IsOpen() const;
        bool IsPauseEnabled() const;
        void SetPauseEnabled(bool enabled);
    private:
        UIManager() = default;
        void LoadSettings();
        void SaveSettings();
        
        void* m_view = nullptr;
        bool m_isOpen = false;
        bool m_pauseGame = false;
    };
}