#include "ui/MagelightBridge.h"
#include "core/StudioService.h"
#include "MagelightUI_Mod.h"
#include <memory>
#include <atomic>

namespace NPCStudio::UI {
using json = nlohmann::json;
static MAGELIGHT_API::MagelightMod Host;
static MAGELIGHT_API::ViewId View = 0;
static std::atomic<std::uint64_t> Epoch = 0;

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
    if (!text || std::strlen(text) > 65536) return;
    auto command = std::make_unique<Command>();
    command->text = text;
    command->epoch = Epoch.load();
    auto* task = command.release();
    if (Host.v4()->PostGameTask(Execute, task) != MAGELIGHT_API::Result::Ok) delete task;
}

void Close() {
    ++Epoch;
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
    Host.onClose(View, [](auto) { ++Epoch; Host.v4()->ShowView(View, false); });
    Host.onError(View, [](auto, const char* error) { logger::error("Ошибка страницы NPCStudio: {}", error); Close(); });
    // F8 свободен от модификаторов и обрабатывается самим владельцем фокуса Magelight.
    Host.v4()->BindHotkey(View, 0x42, MAGELIGHT_API::kHotkeyActionToggleUIMode);
    logger::info("Интерфейс Magelight готов; F8 открывает NPCStudio");
}
}
