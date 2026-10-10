#pragma once
// TODO: Stage 2 — реализовать управление PrismUI Dashboard
// REF: см. IntelEngine-NativePlugin/SKSE/src/DashboardUIManager.cpp
#include <string>

namespace NPCStudio::UI {
    class UIManager {
    public:
        static UIManager& GetSingleton();
        void Register();
        void OpenDashboard();
        void CloseDashboard();
        void ToggleDashboard();
        bool IsOpen() const { return m_isOpen; }
    private:
        UIManager() = default;
        void* m_view = nullptr;
        bool m_isOpen = false;
    };
}