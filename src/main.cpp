#include "pch.h"

#include "Config/Config.h"
#include "App/ActionService.h"
#include "App/Lifecycle.h"
#include "App/Profiler.h"
#include "Core/RuntimeCompatibility.h"
#include "Platform/SteamKeyboard.h"
#include "App/CatalogService.h"
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
        if (!message || ESPExplorerAE::Lifecycle::Shutdown().Requested()) {
            return;
        }

        REX::DEBUG("Received F4SE message type {}", message->type);

        switch (message->type) {
        case F4SE::MessagingInterface::kPreLoadGame:
            ESPExplorerAE::CatalogService::SetAvailable(false);
            ESPExplorerAE::ActionService::BeginSession(false);
            ESPExplorerAE::SteamKeyboard::Abandon();
            ESPExplorerAE::Config::FlushPendingSave();
            break;
        case F4SE::MessagingInterface::kPostLoadGame:
            ESPExplorerAE::ActionService::BeginSession(message->data != nullptr);
            ESPExplorerAE::CatalogService::SetAvailable(message->data != nullptr);
            break;
        case F4SE::MessagingInterface::kNewGame:
            ESPExplorerAE::CatalogService::SetAvailable(false);
            ESPExplorerAE::CatalogService::SetAvailable(true, true);
            ESPExplorerAE::ActionService::BeginSession(true);
            ESPExplorerAE::SteamKeyboard::Abandon();
            break;
        case F4SE::MessagingInterface::kGameDataReady:
            if (!message->data) {
                ESPExplorerAE::CatalogService::SetAvailable(false);
                ESPExplorerAE::ActionService::BeginSession(false);
            } else {
                ESPExplorerAE::CatalogService::SetAvailable(true);
            }
            ESPExplorerAE::Hooks::Install();
            break;
        case F4SE::MessagingInterface::kPostPostLoad:
        case F4SE::MessagingInterface::kGameLoaded:
            ESPExplorerAE::Hooks::Install();
            break;
        case F4SE::MessagingInterface::kPreSaveGame:
            ESPExplorerAE::Config::FlushPendingSave();
            break;
        default:
            break;
        }
    }
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
    // F4SE recognizes the shared 1.11-series address/layout flags. Bound that
    // family to the requested executables before initializing any game bindings.
    if (!a_f4se) {
        return false;
    }
    const auto parts = [](REL::Version version) -> ESPExplorerAE::ExecutableVersion {
        return { version[0], version[1], version[2], version[3] };
    };
    if (!ESPExplorerAE::MeetsRuntimeRequirement(parts(a_f4se->RuntimeVersion()), parts(a_f4se->F4SEVersion()))) return false;

    F4SE::Init(a_f4se);

    if (!ESPExplorerAE::Config::Load()) {
        REX::WARN("Failed to load config");
        return false;
    }

    const auto& settings = ESPExplorerAE::Config::Get();
    SetLogLevel(settings.debugLogging);
    ESPExplorerAE::Profiler::Configure(settings.profilePerformance);
    REX::INFO("Plugin load started");

    if (!ESPExplorerAE::Language::Load(settings.language, [](std::string message) { REX::WARN("{}", message); })) {
        REX::WARN("Failed to load language file");
    }

    const auto* messaging = F4SE::GetMessagingInterface();
    if (!messaging || messaging->Version() < F4SE::MessagingInterface::kVersion ||
        !messaging->RegisterListener(MessageHandler)) {
        REX::WARN("Required F4SE messaging listener unavailable; plugin load failed");
        return false;
    }

    if (!ESPExplorerAE::ActionService::Initialize()) {
        REX::WARN("Game action dispatcher unavailable; gameplay actions disabled");
    }
    ESPExplorerAE::Hooks::Install();

    REX::INFO("ESPExplorerAE initialized");
    return true;
}
