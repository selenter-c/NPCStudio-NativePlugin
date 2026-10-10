// TODO: Stage 2 — реализовать обработку ввода SHIFT+8
// REF: см. IntelEngine-NativePlugin/SKSE/src/ — паттерны BSInputEventSink
#include "input/InputHandler.h"

namespace NPCStudio::Input {

InputHandler* InputHandler::GetSingleton() {
    static InputHandler instance;
    return &instance;
}

bool InputHandler::Register() {
    // TODO: Stage 2 — регистрация через BSInputDeviceManager::AddEventSink
    return false;
}

} // namespace NPCStudio::Input