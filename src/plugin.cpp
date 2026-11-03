#include "PCH.h"
#include "core/StudioService.h"
#include "ui/MagelightBridge.h"
#include <spdlog/sinks/basic_file_sink.h>

namespace {
void Message(SKSE::MessagingInterface::Message* message) {
    auto& studio = NPCStudio::StudioService::Get();
    switch (message->type) {
    case SKSE::MessagingInterface::kDataLoaded:
        studio.Initialize();
        NPCStudio::UI::Initialize();
        break;
    case SKSE::MessagingInterface::kPreLoadGame:
        NPCStudio::UI::Close();
        studio.BeforeLoad();
        break;
    case SKSE::MessagingInterface::kPostLoadGame:
        studio.AfterLoad(reinterpret_cast<std::uintptr_t>(message->data) != 0);
        break;
    case SKSE::MessagingInterface::kNewGame:
        studio.NewGame();
        break;
    }
}
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    auto path = SKSE::log::log_directory();
    if (!path) return false;
    *path /= "NPCStudio.log";
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
    auto log = std::make_shared<spdlog::logger>("NPCStudio", sink);
    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);
    spdlog::set_default_logger(log);
    SKSE::Init(skse, false);
    if (skse->RuntimeVersion() != REL::Version(1, 6, 1170, 0)) {
        logger::error("Поддерживается Skyrim 1.6.1170; получен {}", skse->RuntimeVersion().string());
        return false;
    }
    auto* serialization = SKSE::GetSerializationInterface();
    if (!serialization || !SKSE::GetMessagingInterface()->RegisterListener("SKSE", Message)) return false;
    serialization->SetUniqueID(0x5554534E);
    serialization->SetSaveCallback([](auto* stream) { NPCStudio::StudioService::Get().Save(stream); });
    serialization->SetLoadCallback([](auto* stream) { NPCStudio::StudioService::Get().Load(stream); });
    serialization->SetRevertCallback([](auto*) { NPCStudio::StudioService::Get().Revert(); });
    logger::info("NPCStudio 0.6.0: Magelight, постоянный пул 200+200, сохранение SKSE");
    return true;
}
