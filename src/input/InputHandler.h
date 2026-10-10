#pragma once
#include <RE/B/BSTEvent.h>
#include <RE/I/InputEvent.h>

namespace NPCStudio::Input {
    class InputHandler : public RE::BSTEventSink<RE::InputEvent*> {
    public:
        static InputHandler* GetSingleton();
        bool Register();
        RE::BSEventNotifyControl ProcessEvent(
            RE::InputEvent* const* a_event,
            RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;
    private:
        InputHandler() = default;
        bool m_shiftPressed = false;
    };
}