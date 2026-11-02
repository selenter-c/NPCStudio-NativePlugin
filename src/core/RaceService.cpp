#include "core/RaceService.h"
#include <atomic>
#include <memory>
#include <unordered_map>
namespace NPCStudio::Races {
using Completion = std::shared_ptr<std::atomic<bool>>;
static std::unordered_map<RE::FormID, Completion> Calls;
class Callback : public RE::BSScript::IStackCallbackFunctor {
    Completion completed;
    RE::FormID formId;

  public:
    Callback(Completion value, RE::FormID id) : completed(std::move(value)), formId(id) {
    }
    void operator()(RE::BSScript::Variable) override {
        logger::info("SetRace VM завершён для {:08X}", formId);
        *completed = true;
    }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object> &) override {
    }
};
bool Pending(RE::Actor *actor) {
    const auto it = Calls.find(actor->GetFormID());
    if (it == Calls.end())
        return false;
    if (!it->second->load())
        return true;
    Calls.erase(it);
    return false;
}
void Clear() {
    Calls.clear();
}

static constexpr std::string_view Allowed[] = {"NordRace",     "BretonRace",  "ImperialRace", "RedguardRace",
                                               "HighElfRace",  "WoodElfRace", "DarkElfRace",  "OrcRace",
                                               "ArgonianRace", "KhajiitRace"};
RE::TESRace *Resolve(const std::string &editorId) {
    if (std::ranges::find(Allowed, editorId) == std::end(Allowed))
        throw std::runtime_error("Unsupported race");
    for (auto *race : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESRace>())
        if (race && editorId == race->GetFormEditorID() && !race->IsChildRace())
            return race;
    throw std::runtime_error("Race is unavailable");
}
nlohmann::json List() {
    auto result = nlohmann::json::array();
    for (auto id : Allowed) {
        try {
            auto *race = Resolve(std::string(id));
            result.push_back({{"id", id}, {"name", race->GetName()}});
        } catch (const std::exception &) {
        }
    }
    return result;
}
void Request(RE::Actor *actor, RE::TESRace *race) {
    if (!actor || !race)
        throw std::runtime_error("Race is unavailable");
    if (Pending(actor))
        throw std::runtime_error("A race change is still pending");
    if (actor->GetRace() == race)
        return;
    // Используем штатный SetRace через VM: поле ActorBase не заменяет пересборку скелета и головы.
    auto *vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    auto *policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
    if (!policy)
        throw std::runtime_error("Papyrus VM is unavailable");
    const auto handle = policy->GetHandleForObject(actor->GetFormType(), actor);
    auto completed = std::make_shared<std::atomic<bool>>(false);
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new Callback(completed, actor->GetFormID()));
    if (!vm->DispatchMethodCall(handle, "Actor", "SetRace", RE::MakeFunctionArguments(std::move(race)), callback))
        throw std::runtime_error("Could not schedule Actor.SetRace");
    Calls[actor->GetFormID()] = std::move(completed);
    logger::info("SetRace запрос для {:08X}: {}", actor->GetFormID(), race->GetFormEditorID());
}
} // namespace NPCStudio::Races
