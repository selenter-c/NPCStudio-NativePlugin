#include "PCH.h"
#include "core/StudioService.h"
#include "ui/MagelightBridge.h"
#include <spdlog/sinks/basic_file_sink.h>

namespace {
constexpr REL::Version SupportedRuntimes[] = {
    SKSE::RUNTIME_SSE_1_6_1130,
    SKSE::RUNTIME_SSE_1_6_1170,
    SKSE::RUNTIME_SSE_1_6_1179_GOG,
    SKSE::RUNTIME_SSE_1_7_99,
    SKSE::RUNTIME_SSE_1_7_104,
};

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
    const auto runtime = skse->RuntimeVersion();
    if (std::ranges::find(SupportedRuntimes, runtime) == std::end(SupportedRuntimes)) {
        logger::error("Неподдерживаемый Skyrim {}. Допустимые версии: 1.6.1130, 1.6.1170, 1.6.1179 GOG, 1.7.99, 1.7.104", runtime.string("."));
        return false;
    }
    SKSE::Init(skse, false);
    auto* serialization = SKSE::GetSerializationInterface();
    if (!serialization || !SKSE::GetMessagingInterface()->RegisterListener("SKSE", Message)) return false;
    serialization->SetUniqueID(0x5554534E);
    serialization->SetSaveCallback([](auto* stream) { NPCStudio::StudioService::Get().Save(stream); });
    serialization->SetLoadCallback([](auto* stream) { NPCStudio::StudioService::Get().Load(stream); });
    serialization->SetRevertCallback([](auto*) { NPCStudio::StudioService::Get().Revert(); });
    logger::info("NPCStudio 0.8.11, Skyrim {}, SKSE {}: Magelight, постоянный пул 200+200, сохранение SKSE",
        runtime.string("."), REL::Version::unpack(skse->SKSEVersion()).string("."));
    return true;
}
