#include "input/InputHandler.h"
#include "ui/UIManager.h"
#include <RE/B/BSInputDeviceManager.h>
#include <RE/B/ButtonEvent.h>
#include <spdlog/spdlog.h>

namespace NPCStudio::Input {

InputHandler* InputHandler::GetSingleton() {
    static InputHandler instance;
    return &instance;
}

bool InputHandler::Register() {
    auto* mgr = RE::BSInputDeviceManager::GetSingleton();
    if (!mgr) {
        spdlog::error("BSInputDeviceManager not found");
        return false;
    }
    mgr->AddEventSink(this);
    spdlog::info("Input handler registered (SHIFT+8 to toggle dashboard)");
    return true;
}

RE::BSEventNotifyControl InputHandler::ProcessEvent(
    RE::InputEvent* const* a_event,
    RE::BSTEventSource<RE::InputEvent*>*) {

    if (!a_event) return RE::BSEventNotifyControl::kContinue;

    for (auto* ev = *a_event; ev; ev = ev->next) {
        auto* btn = ev->AsButtonEvent();
        if (!btn) continue;
        if (btn->GetDevice() != RE::INPUT_DEVICE::kKeyboard) continue;

        const auto code = btn->GetIDCode();

        if (code == 0x2A || code == 0x36) {
            m_shiftPressed = btn->IsPressed();
        }

        // DX scancode: '8' = 0x09
        if (code == 0x09 && m_shiftPressed && btn->IsDown()) {
            spdlog::info("SHIFT+8 pressed - toggling dashboard");
            UI::UIManager::GetSingleton().ToggleDashboard();
            return RE::BSEventNotifyControl::kStop;
        }

        // ESC = 0x01
        if (code == 0x01 && btn->IsDown() && UI::UIManager::GetSingleton().IsOpen()) {
            UI::UIManager::GetSingleton().CloseDashboard();
            return RE::BSEventNotifyControl::kStop;
        }
    }
    return RE::BSEventNotifyControl::kContinue;
}

} // namespace NPCStudio::Input