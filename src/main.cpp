#include "pch.h"

#include "Config/Config.h"
#include "Data/DataManager.h"
#include "Hooks/Hooks.h"
#include "Localization/Language.h"

namespace
{
    void MessageHandler(F4SE::MessagingInterface::Message* message)
    {
        if (!message) {
            return;
        }

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
    F4SE::Init(a_f4se);

    if (!ESPExplorerAE::Config::Load()) {
        REX::WARN("Failed to load config");
        return false;
    }

    const auto& settings = ESPExplorerAE::Config::Get();
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
