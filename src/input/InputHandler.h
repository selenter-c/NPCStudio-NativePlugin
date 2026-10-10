#pragma once
// TODO: Stage 2 — реализовать обработку ввода SHIFT+8
// REF: см. IntelEngine-NativePlugin/SKSE/src/ — паттерны обработки ввода
namespace NPCStudio::Input {
    class InputHandler {
    public:
        static InputHandler* GetSingleton();
        bool Register();
    private:
        InputHandler() = default;
        bool m_shiftPressed = false;
    };
}