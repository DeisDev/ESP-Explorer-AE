#include "GUI/Widgets/BasketView.h"
#include "GUI/Widgets/ModalUtils.h"
#include "GUI/Widgets/SharedUtils.h"
#include "GUI/Widgets/SearchBar.h"
#include "GUI/Widgets/ImGuiWidgetUtils.h"
#include "Input/GamepadInput.h"
#include <imgui.h>

namespace ESPExplorerAE
{
    namespace
    {
        const char* IssueText(KitIssue issue, const BrowserView& view)
        {
            const auto& localize = view.localize;
            switch (issue) {
            case KitIssue::Empty: return localize("Workspace", "sBasketEmpty", "Add items from results or an inspector.");
            case KitIssue::Unresolved: return localize("Workspace", "sUnresolved", "Unresolved; retained for later");
            case KitIssue::Ineligible: return localize("Workspace", "sIneligible", "This record cannot be given as a base item.");
            case KitIssue::Quantity: return localize("Workspace", "sInvalidQuantity", "Quantity or aggregated quantity exceeds the allowed range.");
            case KitIssue::Ammo: return localize("Workspace", "sInvalidAmmo", "This entry has no available weapon ammunition. Turn off its ammo option.");
            case KitIssue::Capacity: return localize("Workspace", "sQueueCapacity", "The entire kit does not fit in the action queue. Reduce the kit or wait for queued actions.");
            case KitIssue::Unavailable: return localize("Workspace", "sGameplayRequired", "A loaded, available game session is required.");
            case KitIssue::Stale: return localize("Workspace", "sReviewChanged", "Targets or session changed. Review the kit again.");
            default: return "";
            }
        }
    }
    void DrawBasketWindow(bool& open, BasketViewState& state, ItemKit& kit, const BrowserView& view, std::size_t pending, BasketRequests& requests)
    {
        if (!open) return;
        const auto& localize = view.localize;
        const auto reviewTitle = std::string(localize("Workspace", "sReviewKit", "Review Kit")) + "###ReviewKit";
        const auto title = std::string(localize("Workspace", "sBasket", "Basket")) + "###WorkspaceBasket";
        ModalUtils::PrepareToolWindow(title.c_str(), {800, 680}, {440, 300}, state.focusPending);
        if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse)) {
            if (ModalUtils::EscapeClosesCurrentWindow()) open = false;
            ImGui::TextWrapped("%s", localize("Workspace", "sBasketHint", "Base items only. Ammunition is added once per weapon entry, regardless of the number of copies."));
            if (state.full) ImGui::TextWrapped("%s", IssueText(KitIssue::Capacity, view));
            if (state.submitted) {
                ImGui::TextWrapped("%s", localize("Workspace", "sKitSubmitted", "Kit admitted to the queue once. Check Action History for observed results."));
                if (ImGui::Button(localize("General", "sActionHistory", "Action History"))) requests.history = true;
            }
            if (state.admission != ActionAdmission::Accepted) ImGui::TextWrapped("%s", localize("Workspace", "sKitRejected", "The kit was not admitted. Review current targets and queue availability before trying again."));
            auto preview = ReviewItemKit(kit, *view.catalog, view.session, view.gameplayReady, pending, view.componentSubstitution);
            bool edited{};
            std::optional<std::size_t> remove;
            if (ImGui::BeginChild("BasketEntries", {0, (std::max)(120.0f, ImGui::GetContentRegionAvail().y - 210.0f)}, ImGuiChildFlags_Borders)) {
                for (std::size_t i = 0; i < kit.entries.size(); ++i) {
                    auto& entry = kit.entries[i]; ImGui::PushID(static_cast<int>(i));
                    const auto* row = i < preview.rows.size() ? &preview.rows[i] : nullptr;
                    if (ImGui::Selectable((entry.record.name.empty() ? entry.record.identity : entry.record.name).c_str()) && row && row->formID) requests.inspect.push_back(row->formID);
                    if (entry.record.identity.empty()) ImGui::TextDisabled("%s", localize("Workspace", "sSessionOnly", "Session only; not restored as a target"));
                    if (row && row->issue != KitIssue::None) ImGui::TextWrapped("%s", IssueText(row->issue, view));
                    int quantity = static_cast<int>(entry.quantity);
                    ImGui::SetNextItemWidth(150);
                    if (ImGui::InputInt(localize("General", "sQuantity", "Quantity"), &quantity)) { entry.quantity = static_cast<std::uint32_t>((std::clamp)(quantity, 1, static_cast<int>(ActionQueue::MaxQuantity))); edited = true; }
                    const auto* record = row ? view.catalog->Find(row->formID) : nullptr;
                    if (entry.includeAmmo || (record && record->category == "WEAP")) {
                        edited |= ImGui::Checkbox(localize("Items", "sIncludeAmmo", "Include Ammo"), &entry.includeAmmo);
                        if (entry.includeAmmo) {
                            int ammo = static_cast<int>(entry.ammoQuantity); ImGui::SameLine(); ImGui::SetNextItemWidth(150);
                            if (ImGui::InputInt(localize("Items", "sAmmoQuantity", "Ammo Quantity"), &ammo)) { entry.ammoQuantity = static_cast<std::uint32_t>((std::clamp)(ammo, 0, static_cast<int>(ActionQueue::MaxQuantity))); edited = true; }
                        }
                    }
                    if (ImGui::SmallButton(localize("General", "sRemove", "Remove"))) remove = i;
                    ImGui::Separator(); ImGui::PopID();
                }
            }
            ImGui::EndChild();
            if (remove) { kit.entries.erase(kit.entries.begin() + *remove); edited = true; state.full = false; }
            if (edited) { state.reviewed.reset(); state.submitted = false; preview = ReviewItemKit(kit, *view.catalog, view.session, view.gameplayReady, pending, view.componentSubstitution); }
            ImGui::Text("%s: %zu | %s: %llu | %s: %llu", localize("Workspace", "sEntries", "Entries"), kit.entries.size(),
                localize("Workspace", "sItemTotal", "Item Total"), static_cast<unsigned long long>(preview.itemTotal),
                localize("Workspace", "sAmmoTotal", "Ammo Total"), static_cast<unsigned long long>(preview.ammoTotal));
            if (!preview) ImGui::TextWrapped("%s", IssueText(preview.issue, view));
            ImGui::InputText(localize("Workspace", "sKitName", "Kit Name"), state.name.data(), state.name.size());
            SearchBar::ReadControllerText(localize("Workspace", "sKitName", "Kit Name"), state.name.data(), state.name.size());
            ImGui::BeginDisabled(!preview);
            if (ImGui::Button(localize("Workspace", "sReviewKit", "Review Kit"))) { state.reviewed = preview; ImGui::OpenPopup(reviewTitle.c_str()); }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button(localize("Workspace", "sClearBasket", "Clear Basket"))) ImGui::OpenPopup("ClearBasket");
            if (ImGui::BeginPopup("ClearBasket")) {
                ImGui::TextWrapped("%s", localize("Workspace", "sClearBasketConfirm", "Remove all basket entries? Saved kits are kept."));
                if (ImGui::Button(localize("General", "sRemove", "Remove"))) { kit.entries.clear(); state.reviewed.reset(); state.submitted = false; ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }
            if (state.saveFailed) ImGui::TextWrapped("%s", localize("Workspace", "sSaveFailed", "Changes could not be saved. Check names, input limits, and workspace file access."));
            const auto* viewport = ImGui::GetMainViewport();
            const ImVec2 maximum{viewport->WorkSize.x - 24, viewport->WorkSize.y - 24};
            const ModalUtils::PopupSizing sizing({(std::min)(620.0f, maximum.x), (std::min)(500.0f, maximum.y)}, {(std::min)(400.0f, maximum.x), (std::min)(300.0f, maximum.y)}, maximum, false);
            if (ImGui::BeginPopupModal(reviewTitle.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings)) {
                if (!GamepadInput::IsSteamKeyboardOpen() && ModalUtils::CancelPopupRequested()) ImGui::CloseCurrentPopup();
                ImGui::TextWrapped("%s", localize("Workspace", "sReviewKitHint", "Review the aggregated base items below. This admits the whole request list or none. Once execution starts, results are independent and are never automatically retried."));
                const bool matches = state.reviewed && SameKitReview(*state.reviewed, preview);
                if (!matches) ImGui::TextWrapped("%s", IssueText(KitIssue::Stale, view));
                if (ImGui::BeginChild("AggregatedKit", {0, -ImGui::GetFrameHeightWithSpacing() * 3}, ImGuiChildFlags_Borders)) {
                    if (state.reviewed) for (const auto& action : state.reviewed->actions) {
                        const auto* record = view.catalog->Find(action.formID);
                        ImGui::Text("%s x%u", record ? record->name.c_str() : localize("General", "sUnavailable", "Unavailable"), action.count);
                    }
                }
                ImGui::EndChild();
                if (ImGui::Button(localize("General", "sCancel", "Cancel"))) ImGui::CloseCurrentPopup();
                ImGuiWidgetUtils::DrawWrappedSameLine(localize("Workspace", "sExecuteOnce", "Give Reviewed Kit Once")); ImGui::BeginDisabled(!matches || state.submitted);
                if (ImGui::Button(localize("Workspace", "sExecuteOnce", "Give Reviewed Kit Once"))) { requests.execute = *state.reviewed; ImGui::CloseCurrentPopup(); }
                ImGui::EndDisabled(); ImGuiWidgetUtils::DrawWrappedSameLine(localize("Workspace", "sSaveKit", "Save Reviewed Kit")); ImGui::BeginDisabled(!matches || !state.name[0]);
                if (ImGui::Button(localize("Workspace", "sSaveKit", "Save Reviewed Kit"))) { auto saved = kit; saved.name = state.name.data(); requests.save = std::move(saved); ImGui::CloseCurrentPopup(); }
                ImGui::EndDisabled();
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }
}
