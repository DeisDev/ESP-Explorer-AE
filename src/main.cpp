#include "pch.h"

#include "Config/Config.h"
#include "Data/DataManager.h"
#include "Hooks/Hooks.h"
#include "Localization/Language.h"

#include <spdlog/spdlog.h>

// CommonLibF4 generates the loader's compatible-runtime list from RUNTIME_LATEST.
// A dependency update must not silently advertise support for an untested runtime.
static_assert(F4SE::RUNTIME_LATEST == F4SE::RUNTIME_1_11_240);

namespace
{
    void SetLogLevel(bool debug)
    {
        const auto level = debug ? spdlog::level::debug : spdlog::level::info;
        spdlog::set_level(level);
        spdlog::default_logger()->flush_on(level);
    }

    void MessageHandler(F4SE::MessagingInterface::Message* message)
    {
        if (!message) {
            return;
        }

        REX::DEBUG("Received F4SE message type {}", message->type);

        switch (message->type) {
        case F4SE::MessagingInterface::kPostPostLoad:
        case F4SE::MessagingInterface::kGameLoaded:
        case F4SE::MessagingInterface::kPostLoadGame:
        case F4SE::MessagingInterface::kGameDataReady:
            ESPExplorerAE::DataManager::Refresh();
            ESPExplorerAE::Hooks::Install();
            break;
        default:
            break;
        }
    }
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
    // Address Library/layout flags can bypass F4SE's compatible-version list.
    // Reject other runtimes before CommonLib initialization or any game access.
    if (!a_f4se || a_f4se->RuntimeVersion() != F4SE::RUNTIME_1_11_240) {
        return false;
    }

    F4SE::Init(a_f4se);

    if (!ESPExplorerAE::Config::Load()) {
        REX::WARN("Failed to load config");
        return false;
    }

    const auto& settings = ESPExplorerAE::Config::Get();
    SetLogLevel(settings.debugLogging);
    REX::INFO("Plugin load started");

    if (!ESPExplorerAE::Language::Load(settings.language)) {
        REX::WARN("Failed to load language file");
    }

    if (const auto* messaging = F4SE::GetMessagingInterface()) {
        if (!messaging->RegisterListener(MessageHandler)) {
            REX::WARN("Failed to register F4SE messaging listener");
        }
    }

    ESPExplorerAE::Hooks::Install();
    ESPExplorerAE::DataManager::Refresh();

    REX::INFO("ESPExplorerAE initialized");
    return true;
}
