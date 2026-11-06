#include "ui/SettingsCodec.h"
#include "preview/ActorPreview.h"
#include "ui/MagelightBridge.h"
#include "core/StudioService.h"
#include "MagelightUI_Mod.h"
#include <memory>
#include <atomic>
#include <fstream>

namespace NPCStudio::UI {
using json = nlohmann::json;
static MAGELIGHT_API::MagelightMod Host;
static MAGELIGHT_API::ViewId View = 0;
static std::atomic<std::uint64_t> Epoch = 0;
static bool WorldWork = false;
void SetWorldWork(bool pending) {
    if (WorldWork == pending) return;
    WorldWork = pending;
    if (Host && View && Host.v4()->GetUIModeOwner() == Host.id())
        Host.v4()->RequestUIMode(View, pending ? 0 : MAGELIGHT_API::kUIModeFlagPause);
}

static std::atomic<unsigned> Hotkey = 0x09;
static std::atomic<unsigned> Modifiers = 1;
static std::string Language = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN ? "ru" : "en";
static std::string Theme = "ember";
static double Scale = 1.;
static constexpr auto SettingsPath = L"Data/SKSE/Plugins/NPCStudio/settings.json";

nlohmann::json Settings() { return {{"schema", 2}, {"key", Hotkey.load()}, {"modifiers", Modifiers.load()}, {"language", Language}, {"theme", Theme}, {"scale", Scale}}; }
void SaveSettings(const nlohmann::json& settings) {
    const auto saved = NormalizeSettings(settings, Settings());
    const auto key = saved.at("key").get<unsigned>(), modifiers = saved.at("modifiers").get<unsigned>();
    const auto language = saved.at("language").get<std::string>(), theme = saved.at("theme").get<std::string>();
    std::filesystem::create_directories(std::filesystem::path(SettingsPath).parent_path());
    const auto temp = std::filesystem::path(std::wstring(SettingsPath) + L".tmp");
    { std::ofstream file(temp); file << saved.dump(2); file.flush(); if (!file) throw std::runtime_error("Не удалось записать настройки"); }
    if (!MoveFileExW(temp.c_str(), SettingsPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Не удалось сохранить настройки");
    Hotkey = key; Modifiers = modifiers; Language = language; Theme = theme;
    Scale = saved.at("scale").get<double>();
}

static void Toggle(void*) {
    if (!Host || !View) return;
    if (Host.v4()->GetUIModeOwner() == Host.id()) { Close(); return; }
    auto* ui = RE::UI::GetSingleton();
    auto state = StudioService::Get().Execute({{"action", "state"}});
    if (!state.value("worldLoaded", false) || !ui || ui->GameIsPaused() || ui->IsMenuOpen(RE::Console::MENU_NAME) ||
        ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME) || ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME)) return;
    StudioService::Get().PickAppearanceTarget();
    Host.v4()->RequestUIMode(View, WorldWork ? 0 : MAGELIGHT_API::kUIModeFlagPause);
}
class HotkeySink : public RE::BSTEventSink<RE::InputEvent*> {
public:
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events, RE::BSTEventSource<RE::InputEvent*>*) override {
        if (!events) return RE::BSEventNotifyControl::kContinue;
        for (auto* event = *events; event; event = event->next) {
            auto* button = event->AsButtonEvent();
            if (!button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard || !button->IsDown() || button->GetIDCode() != Hotkey.load()) continue;
            const unsigned modifiers = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 1u : 0u) |
                ((GetAsyncKeyState(VK_CONTROL) & 0x8000) ? 2u : 0u) |
                ((GetAsyncKeyState(VK_MENU) & 0x8000) ? 4u : 0u);
            if (modifiers == Modifiers.load() && Host) Host.v4()->PostGameTask(Toggle, nullptr);
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};
static HotkeySink InputSink;

struct Command { std::string text; std::uint64_t epoch = 0; };

static void Execute(void* data) {
    std::unique_ptr<Command> command(static_cast<Command*>(data));
    if (command->epoch != Epoch.load()) return;
    json response;
    response["requestId"] = nullptr;
    try {
        auto request = json::parse(command->text);
        response["requestId"] = request.value("requestId", json(nullptr));
        if (request.at("action") == "close") { Close(); return; }
        response["state"] = StudioService::Get().Execute(request);
        response["ok"] = true;
    } catch (const std::exception& e) {
        response["ok"] = false;
        response["error"] = e.what();
        logger::warn("Команда интерфейса отклонена: {}", e.what());
    }
    Host.v4()->InteropCall(View, "studioResponse", response.dump().c_str());
}

static void OnCommand(MAGELIGHT_API::ViewId, const char* text, void*) {
    if (!text || std::strlen(text) > 2100000) return;
    auto command = std::make_unique<Command>();
    command->text = text;
    command->epoch = Epoch.load();
    auto* task = command.release();
    if (Host.v4()->PostGameTask(Execute, task) != MAGELIGHT_API::Result::Ok) delete task;
}

struct PreviewUpdate { json value; std::uint64_t epoch; };
void PublishPreview(json update) {
    if (!Host || !View) return;
    auto task = std::make_unique<PreviewUpdate>(PreviewUpdate{std::move(update), Epoch.load()});
    auto callback = [](void* data) {
        std::unique_ptr<PreviewUpdate> update(static_cast<PreviewUpdate*>(data));
        if (update->epoch != Epoch.load() || !Preview::IsCurrent(update->value.at("generation").get<std::uint64_t>())) return;
        Host.v4()->InteropCall(View, "studioPreview", update->value.dump().c_str());
    };
    auto* raw = task.release();
    if (Host.v4()->PostGameTask(callback, raw) != MAGELIGHT_API::Result::Ok) delete raw;
}
void Close() {
    ++Epoch;
    Preview::Clear();
    StudioService::Get().CancelAppearance();
    if (!Host || !View) return;
    Host.v4()->ReleaseUIMode(Host.id());
    Host.v4()->ShowView(View, false);
}

void Initialize() {
    if (!Host.acquire("NPCStudio", "NPC Studio", 1, 5, 0)) {
        logger::error("Для интерфейса требуется Magelight UI 1.5.0+");
        return;
    }
    wchar_t exe[32768]{};
    GetModuleFileNameW(nullptr, exe, static_cast<DWORD>(std::size(exe)));
    auto path = std::filesystem::path(exe).parent_path() / L"Data/Magelight/NPCStudio/views/studio/index.html";
    auto utf8 = path.u8string();
    std::string page(reinterpret_cast<const char*>(utf8.data()), utf8.size());
    MAGELIGHT_API::MagelightMod::ViewOpts options;
    options.fullscreen = true;
    View = Host.createView("studio", page.c_str(), MAGELIGHT_API::Layer::Panel, options);
    if (!View) { logger::error("Окно NPCStudio не создано: {}", Host.v4()->GetLastErrorMessage(Host.id())); return; }
    Host.v4()->RegisterJSListenerEx(View, "studioCommand", OnCommand, nullptr);
    Host.onClose(View, [](auto) {
        ++Epoch;
        Preview::Clear();
        Host.v4()->PostGameTask([](void*) {
            if (Host.v4()->GetUIModeOwner() != Host.id()) StudioService::Get().CancelAppearance();
        }, nullptr);
        Host.v4()->ShowView(View, false);
    });
    Host.onError(View, [](auto, const char* error) { logger::error("Ошибка страницы NPCStudio: {}", error); Close(); });
    try {
        auto settings = Settings();
        if (std::filesystem::exists(SettingsPath)) {
            std::ifstream file(SettingsPath); settings = json::parse(file);
            settings = NormalizeSettings(settings, Settings());
        }
        SaveSettings(settings);
    } catch (const std::exception& e) { logger::warn("Настройки интерфейса: {}", e.what()); }
    if (auto* input = RE::BSInputDeviceManager::GetSingleton()) input->AddEventSink(&InputSink);
    logger::info("Интерфейс готов; клавиша {}, модификаторы {} (по умолчанию Shift+8)", Hotkey.load(), Modifiers.load());
}
}
