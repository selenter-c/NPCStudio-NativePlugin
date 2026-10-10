#include "PCH.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <filesystem>
#include "ui/UIManager.h"
#include "input/InputHandler.h"

namespace NPCStudio {
    constexpr const char* PLUGIN_NAME = "NPC Studio";
    constexpr const char* PLUGIN_VERSION = "0.1.0";
    constexpr const char* PLUGIN_AUTHOR = "Selenter";
    constexpr const char* LOG_FILE = "NPCStudio.log";

    void InitializeLog() {
        auto path = logger::log_directory();
        if (!path) {
            return;
        }

        *path /= LOG_FILE;

        try {
            std::filesystem::create_directories(path->parent_path());

            auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
            auto log = std::make_shared<spdlog::logger>("global", std::move(sink));

            log->set_level(spdlog::level::info);
            log->flush_on(spdlog::level::info);
            log->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

            spdlog::set_default_logger(std::move(log));
        } catch (...) {
        }
    }

    void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
        switch (a_msg->type) {
            case SKSE::MessagingInterface::kDataLoaded:
                logger::info("DataLoaded - all game data loaded");
                NPCStudio::UI::UIManager::GetSingleton().Register();
                NPCStudio::Input::InputHandler::GetSingleton()->Register();
                logger::info("NPC Studio ready");
                break;

            case SKSE::MessagingInterface::kNewGame:
                logger::info("NewGame started");
                break;

            case SKSE::MessagingInterface::kPostLoadGame:
                logger::info("PostLoadGame - save loaded");
                break;

            default:
                break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    SKSE::Init(a_skse);

    NPCStudio::InitializeLog();

    logger::info("========================================");
    logger::info("  {} v{}", NPCStudio::PLUGIN_NAME, NPCStudio::PLUGIN_VERSION);
    logger::info("  Author: {}", NPCStudio::PLUGIN_AUTHOR);
    logger::info("  Target: Skyrim SE 1.6.1170");
    logger::info("========================================");
    logger::info("Plugin loading...");

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener("SKSE", NPCStudio::MessageHandler)) {
        logger::error("Failed to register SKSE message listener");
        return false;
    }

    logger::info("SKSE message listener registered");
    logger::info("NPC Studio loaded successfully!");

    return true;
}