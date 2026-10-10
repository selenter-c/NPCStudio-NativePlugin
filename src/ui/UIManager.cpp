// TODO: Stage 2 — реализовать управление PrismUI Dashboard
// REF: см. IntelEngine-NativePlugin/SKSE/src/DashboardUIManager.cpp
#include "ui/UIManager.h"
#include "api/PrismUIBridge.h"

namespace NPCStudio::UI {

UIManager& UIManager::GetSingleton() {
    static UIManager instance;
    return instance;
}

void UIManager::Register() {
    // TODO: Stage 2 — инициализация PrismUI, создание view, регистрация JS listeners
}

void UIManager::OpenDashboard() {
    // TODO: Stage 2 — показ view через PrismUIBridge
    m_isOpen = true;
}

void UIManager::CloseDashboard() {
    // TODO: Stage 2 — скрытие view через PrismUIBridge
    m_isOpen = false;
}

void UIManager::ToggleDashboard() {
    if (m_isOpen) CloseDashboard();
    else OpenDashboard();
}

} // namespace NPCStudio::UI