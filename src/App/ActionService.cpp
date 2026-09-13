#include "App/ActionService.h"
#include "App/Lifecycle.h"
#include "Core/Profiling.h"
#include "App/OverlayController.h"
#include "Game/HostLifecycle.h"
#include "Hooks/Hooks.h"
#include "App/DetailService.h"
#include "App/InventoryService.h"
#include "App/PlayerStatusService.h"
#include "Game/ActionExecutor.h"
#include "Game/OverlayEffects.h"
#include "Game/SettingsReader.h"
#include "App/CatalogService.h"
#include "pch.h"

#include <chrono>
#include <mutex>
#include <thread>

namespace ESPExplorerAE
{
    namespace
    {
        std::mutex stateMutex;
        // A lifecycle callback waits for an in-progress action before allowing
        // the host to begin loading. No request executes across this transition.
        // Console dispatch can synchronously invoke a lifecycle callback on the
        // task thread. Deliberate recursion permits that callback to invalidate
        // the session; executor checkpoints then stop the old batch.
        std::recursive_mutex executionMutex;
        ActionQueue queue;
        bool initialized{};
        bool sessionAvailable{};
        bool substituteComponents{ true };
        bool allowMainMenu{};
        std::uint64_t nextReceipt{ 1 };
        std::uint64_t nextGroup{ 1 };
        std::vector<ActionReceipt> receipts;
    }

    bool ActionService::Initialize()
    {
        if (initialized) return true;
        const auto* tasks = F4SE::GetTaskInterface();
        if (!tasks || tasks->Version() < F4SE::TaskInterface::kVersion) return false;
        tasks->AddTaskPermanent([] { Pump(); });
        initialized = true;
        return true;
    }

    void ActionService::BeginSession(bool mayBecomeReady)
    {
        std::lock_guard executionLock(executionMutex);
        if (Lifecycle::Shutdown().Requested()) return;
        OverlayEffects::EndSession();
        DetailService::ResetSession();
        InventoryService::ResetSession();
        PlayerStatusService::ResetSession();
        std::lock_guard lock(stateMutex);
        sessionAvailable = mayBecomeReady;
        queue.BeginSession(false);
        PerformanceProfile().Observe(ProfileGauge::ActionQueue, 0);
        receipts.clear();
    }

    void ActionService::UpdatePolicy(bool substitute, bool debugOverride)
    {
        std::lock_guard lock(stateMutex);
        substituteComponents = substitute;
        allowMainMenu = debugOverride;
    }

    bool ActionService::Submit(ActionRequest request)
    {
        std::lock_guard lock(stateMutex);
        if (Lifecycle::Shutdown().Requested()) return false;
        request.substituteComponents = substituteComponents;
        request.debugOverride = allowMainMenu;
        if (!request.session) request.session = queue.Session();
        const bool accepted = queue.Submit(request);
        PerformanceProfile().Observe(ProfileGauge::ActionQueue, queue.Size());
        if (!accepted) {
            request.inventory.reset();
            receipts.push_back({ nextReceipt++, queue.Session(), std::move(request), {} });
            if (receipts.size() > 512) receipts.erase(receipts.begin());
        }
        return accepted;
    }

    std::uint64_t ActionService::Session()
    {
        std::lock_guard lock(stateMutex);
        return queue.Session();
    }

    ActionAdmission ActionService::SubmitBatch(std::vector<ActionRequest> requests)
    {
        std::lock_guard lock(stateMutex);
        if (Lifecycle::Shutdown().Requested()) return ActionAdmission::Unavailable;
        const auto group = std::ranges::any_of(requests, [](const auto& request) { return !request.groupName.empty(); }) ? nextGroup++ : 0;
        for (auto& request : requests) {
            request.groupID = group;
            request.substituteComponents = substituteComponents;
            request.debugOverride = allowMainMenu;
            if (!request.session) request.session = queue.Session();
        }
        const auto result = queue.SubmitBatch(requests);
        PerformanceProfile().Observe(ProfileGauge::ActionQueue, queue.Size());
        if (result != ActionAdmission::Accepted) REX::WARN("Batch admission rejected: requests={}, queued={}, reason={}", requests.size(), queue.Size(), static_cast<int>(result));
        return result;
    }

    bool ActionService::IsReady()
    {
        std::lock_guard lock(stateMutex);
        return !Lifecycle::Shutdown().Requested() && queue.IsReady();
    }

    bool ActionService::GodModeEnabled() { return OverlayEffects::GodModeEnabled(); }

    std::size_t ActionService::PendingCount()
    {
        std::lock_guard lock(stateMutex);
        return queue.Size();
    }

    std::vector<ActionReceipt> ActionService::Receipts()
    {
        std::lock_guard lock(stateMutex);
        return receipts;
    }

    void ActionService::Pump()
    {
        auto& shutdown = Lifecycle::Shutdown();
        if (shutdown.Complete()) return; // Permanent F4SE task remains a harmless process-lifetime entry point.
        std::lock_guard executionLock(executionMutex);
        try {
            static bool loggedThread{};
            if (!loggedThread) {
                REX::INFO("Game action task context: thread {}", GetCurrentThreadId());
                loggedThread = true;
            }
            if (!shutdown.Requested() && IsHostQuitRequested() && shutdown.Request()) REX::INFO("Host quit observed; controlled shutdown requested");
            if (shutdown.Requested()) {
                shutdown.PumpGame([] {
                    OverlayController::SetVisible(false);
                    {
                        std::lock_guard lock(stateMutex);
                        sessionAvailable = false;
                        queue.BeginSession(false);
                        PerformanceProfile().Observe(ProfileGauge::ActionQueue, 0);
                        receipts.clear();
                    }
                    CatalogService::SetAvailable(false);
                    DetailService::ResetSession();
                    InventoryService::ResetSession();
                    PlayerStatusService::ResetSession();
                }, [] {
                    InventoryService::Pump(Session(), false, false);
                    return OverlayEffects::RestoreForShutdown();
                });
                if (shutdown.TryFinish([] { return Hooks::RestoreForShutdown(); })) REX::INFO("Controlled shutdown completed; entry points remain resident");
                return;
            }
            bool available;
            bool debugOverride;
            {
                std::lock_guard lock(stateMutex);
                available = sessionAvailable;
                debugOverride = allowMainMenu;
            }
            const bool ready = available && ActionExecutor::AreGameplayActionsAllowed(debugOverride);
            {
                std::lock_guard lock(stateMutex);
                queue.SetReady(ready);
            }
            OverlayEffects::Update(ready);
            SettingsReader::Pump();
            PlayerStatusService::Pump(Session(), ready);
            CatalogService::Pump(ready);
            DetailService::Pump(Session(), CatalogService::Read());
            InventoryService::Pump(Session(), ready, PendingCount() != 0);
            if (!ready) return;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2);
            for (std::size_t count = 0; count < 8 && !shutdown.Requested() && std::chrono::steady_clock::now() < deadline; ++count) {
                std::optional<ActionRequest> request;
                {
                    std::lock_guard lock(stateMutex);
                    request = queue.Pop();
                    PerformanceProfile().Observe(ProfileGauge::ActionQueue, queue.Size());
                }
                if (!request) break;
                ActionOutcome outcome{ .status = ActionStatus::Failed };
                try {
                    outcome = ActionExecutor::Execute(*request, [&] {
                        if (IsHostQuitRequested() && shutdown.Request()) REX::INFO("Host quit observed during action; controlled shutdown requested");
                        return !shutdown.Requested() && request->session == Session();
                    });
                } catch (const std::exception& error) {
                    REX::WARN("Action execution failed: {}", error.what());
                } catch (...) {
                    REX::WARN("Action execution failed with an unknown exception");
                }
                if (outcome.status != ActionStatus::Rejected && outcome.status != ActionStatus::NoChange) InventoryService::Invalidate();
                std::lock_guard lock(stateMutex);
                if (request->session != queue.Session()) break;
                request->inventory.reset(); // Receipts retain outcomes, not complete inventory snapshots.
                receipts.push_back({ nextReceipt++, request->session, std::move(*request), std::move(outcome) });
                if (receipts.size() > 512) receipts.erase(receipts.begin());
            }
        } catch (const std::exception& error) {
            REX::WARN("Action pump failed: {}", error.what());
        } catch (...) {
            REX::WARN("Action pump failed with an unknown exception");
        }
    }
}
