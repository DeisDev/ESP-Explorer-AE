#include "GUI/Widgets/BasketView.h"
#include "GUI/Widgets/FormatUtils.h"
#include "GUI/Widgets/ModalUtils.h"
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
            case KitIssue::Stale: return localize("Workspace", "sReviewChanged", "The basket or game changed. Close this and try again.");
            default: return "";
            }
        }

        void DrawEntries(ItemKit& kit, const KitReview& preview, const BrowserView& view, float height,
            bool& edited, std::optional<std::size_t>& remove, BasketRequests& requests)
        {
            const auto& localize = view.localize;
            const float font = ImGui::GetFontSize();
            const auto* removeLabel = localize("General", "sRemove", "Remove");
            constexpr auto flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY;
            if (!ImGui::BeginTable("BasketTable", 4, flags, {0, height})) return;
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn(localize("General", "sName", "Name"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(localize("General", "sQuantity", "Quantity"), ImGuiTableColumnFlags_WidthFixed, font * 7.5f);
            ImGui::TableSetupColumn(localize("Items", "sAmmo", "Ammo"), ImGuiTableColumnFlags_WidthFixed, font * 7.5f);
            ImGui::TableSetupColumn("##Remove", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize(removeLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f);
            ImGui::TableHeadersRow();
            for (std::size_t i = 0; i < kit.entries.size(); ++i) {
                auto& entry = kit.entries[i];
                const auto* row = i < preview.rows.size() ? &preview.rows[i] : nullptr;
                const auto* record = row && row->formID ? view.catalog->Find(row->formID) : nullptr;
                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                const auto& name = !entry.record.name.empty() ? entry.record.name : entry.record.identity;
                ImGui::AlignTextToFramePadding();
                if (ImGui::Selectable(name.empty() ? localize("General", "sUnnamed", "<Unnamed>") : name.c_str(), false, ImGuiSelectableFlags_AllowOverlap) && record)
                    requests.inspect.push_back(record->formID);
                if (record && ImGui::IsItemHovered()) ImGui::SetTooltip("%s [%s]", record->sourcePlugin.c_str(), FormatUtils::FormID(record->formID).c_str());
                ImGui::PushTextWrapPos(0.0f);
                if (row && row->issue != KitIssue::None) ImGui::TextDisabled("%s", IssueText(row->issue, view));
                else if (entry.record.identity.empty()) ImGui::TextDisabled("%s", localize("Workspace", "sSessionOnly", "Session only; not restored as a target"));
                ImGui::PopTextWrapPos();

                ImGui::TableNextColumn();
                int quantity = static_cast<int>(entry.quantity);
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::InputInt("##Quantity", &quantity)) {
                    entry.quantity = static_cast<std::uint32_t>((std::clamp)(quantity, 1, static_cast<int>(ActionQueue::MaxQuantity)));
                    edited = true;
                }

                ImGui::TableNextColumn();
                if (entry.includeAmmo || (record && record->category == "WEAP")) {
                    edited |= ImGui::Checkbox("##IncludeAmmo", &entry.includeAmmo);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", localize("Workspace", "sAmmoHint", "Adds ammo once for this row, not per copy."));
                    if (entry.includeAmmo) {
                        int ammo = static_cast<int>(entry.ammoQuantity);
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::InputInt("##AmmoQuantity", &ammo, 0, 0)) {
                            entry.ammoQuantity = static_cast<std::uint32_t>((std::clamp)(ammo, 0, static_cast<int>(ActionQueue::MaxQuantity)));
                            edited = true;
                        }
                    }
                } else ImGui::TextDisabled("-");

                ImGui::TableNextColumn();
                if (ImGui::SmallButton(removeLabel)) remove = i;
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    void DrawBasketWindow(bool& open, BasketViewState& state, ItemKit& kit, std::span<const ItemKit> savedKits, bool writable,
        const BrowserView& view, std::size_t pending, BasketRequests& requests)
    {
        if (!open) return;
        const auto& localize = view.localize;
        const auto title = std::string(localize("Workspace", "sBasket", "Basket")) + "###WorkspaceBasket";
        const auto confirmTitle = std::string(localize("Workspace", "sGiveAll", "Give All")) + "###GiveKit";
        ModalUtils::PrepareToolWindow(title.c_str(), {720, 560}, {440, 300}, state.focusPending);
        if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse)) {
            if (ModalUtils::EscapeClosesCurrentWindow()) open = false;
            const auto& style = ImGui::GetStyle();
            const float font = ImGui::GetFontSize();
            auto preview = ReviewItemKit(kit, *view.catalog, view.session, view.gameplayReady, pending, view.componentSubstitution);
            const float footer = ImGui::GetTextLineHeightWithSpacing() * 2.0f + ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.y;
            const float listHeight = (std::max)(ImGui::GetFrameHeight() * 3.0f, ImGui::GetContentRegionAvail().y - footer);
            bool edited{};
            std::optional<std::size_t> remove;
            if (kit.entries.empty()) {
                if (ImGui::BeginChild("BasketEmpty", {0, listHeight}, ImGuiChildFlags_Borders)) {
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextDisabled("%s", IssueText(KitIssue::Empty, view));
                    ImGui::PopTextWrapPos();
                }
                ImGui::EndChild();
            } else DrawEntries(kit, preview, view, listHeight, edited, remove, requests);
            if (remove) { kit.entries.erase(kit.entries.begin() + *remove); edited = true; state.full = false; }
            if (edited) {
                state.reviewed.reset(); state.submitted = false;
                preview = ReviewItemKit(kit, *view.catalog, view.session, view.gameplayReady, pending, view.componentSubstitution);
            }

            ImGui::Text("%s: %llu  |  %s: %llu", localize("Workspace", "sItemTotal", "Item Total"), static_cast<unsigned long long>(preview.itemTotal),
                localize("Workspace", "sAmmoTotal", "Ammo Total"), static_cast<unsigned long long>(preview.ammoTotal));
            bool first = true;
            ImGui::BeginDisabled(!preview || state.submitted);
            if (ImGuiWidgetUtils::DrawWrappedButton(localize("Workspace", "sGiveAll", "Give All"), first)) {
                state.reviewed = preview;
                ImGui::OpenPopup(confirmTitle.c_str());
            }
            ImGui::EndDisabled();
            ImGui::BeginDisabled(kit.entries.empty() || !writable);
            if (ImGuiWidgetUtils::DrawWrappedButton(localize("Workspace", "sSaveAsKit", "Save as Kit"), first)) ImGui::OpenPopup("SaveKit");
            ImGui::EndDisabled();
            if (ImGuiWidgetUtils::DrawWrappedButton(localize("Workspace", "sItemKits", "Item Kits"), first)) ImGui::OpenPopup("KitMenu");
            ImGui::BeginDisabled(kit.entries.empty());
            if (ImGuiWidgetUtils::DrawWrappedButton(localize("Workspace", "sClearBasket", "Clear Basket"), first)) ImGui::OpenPopup("ClearBasket");
            ImGui::EndDisabled();

            if (ImGui::BeginPopup("SaveKit")) {
                if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
                ImGui::SetNextItemWidth(font * 16.0f);
                const bool enter = ImGui::InputTextWithHint("##KitName", localize("Workspace", "sKitName", "Kit Name"), state.name.data(), state.name.size(), ImGuiInputTextFlags_EnterReturnsTrue);
                SearchBar::ReadControllerText(localize("Workspace", "sKitName", "Kit Name"), state.name.data(), state.name.size());
                const bool exists = std::ranges::any_of(savedKits, [&](const ItemKit& saved) { return FoldSearchText(saved.name) == FoldSearchText(state.name.data()); });
                const bool valid = state.name[0] != '\0';
                ImGui::BeginDisabled(!valid);
                if (ImGui::Button(exists ? localize("Workspace", "sReplaceKit", "Replace Kit") : localize("Workspace", "sSave", "Save")) || (enter && valid)) {
                    auto saved = kit;
                    saved.name = state.name.data();
                    requests.save = std::move(saved);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
                ImGui::EndPopup();
            }
            if (ImGui::BeginPopup("KitMenu")) {
                if (savedKits.empty()) ImGui::TextDisabled("%s", localize("Workspace", "sNoKits", "No saved kits."));
                for (std::size_t i = 0; i < savedKits.size(); ++i) {
                    const auto& saved = savedKits[i];
                    ImGui::PushID(static_cast<int>(i));
                    if (ImGui::BeginMenu((saved.name + " (" + std::to_string(saved.entries.size()) + ")").c_str())) {
                        if (kit.entries.empty()) {
                            if (ImGui::MenuItem(localize("Workspace", "sLoadKit", "Load"))) { requests.load = saved; requests.replace = true; }
                        } else {
                            if (ImGui::MenuItem(localize("Workspace", "sAddToBasket", "Add to Basket"))) { requests.load = saved; requests.replace = false; }
                            if (ImGui::MenuItem(localize("Workspace", "sReplaceBasket", "Replace Basket"))) { requests.load = saved; requests.replace = true; }
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem(localize("Workspace", "sDelete", "Delete"), nullptr, false, writable)) requests.removeKit = saved.name;
                        ImGui::EndMenu();
                    }
                    ImGui::PopID();
                }
                ImGui::EndPopup();
            }
            if (ImGui::BeginPopup("ClearBasket")) {
                ImGuiWidgetUtils::PushPopupTextWrap();
                ImGui::TextWrapped("%s", localize("Workspace", "sClearBasketConfirm", "Remove all basket entries? Saved kits are kept."));
                ImGui::PopTextWrapPos();
                if (ImGui::Button(localize("General", "sRemove", "Remove"))) { kit.entries.clear(); state.reviewed.reset(); state.submitted = false; state.full = false; ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }

            ImGuiWidgetUtils::DrawStatusArea("##BasketFeedback", [&] {
                if (state.full) ImGui::TextWrapped("%s", IssueText(KitIssue::Capacity, view));
                if (state.submitted) ImGui::TextWrapped("%s", localize("Workspace", "sKitSubmitted", "Kit queued. Check Action History for results."));
                if (state.admission != ActionAdmission::Accepted) ImGui::TextWrapped("%s", localize("Workspace", "sKitRejected", "Kit not queued. Try again when pending actions finish."));
                if (!preview && preview.issue != KitIssue::Empty) ImGui::TextWrapped("%s", IssueText(preview.issue, view));
                if (state.saveFailed) ImGui::TextWrapped("%s", localize("Workspace", "sSaveFailed", "Changes could not be saved. Check names, input limits, and workspace file access."));
            });

            const auto* viewport = ImGui::GetMainViewport();
            const ImVec2 maximum{viewport->WorkSize.x - 24, viewport->WorkSize.y - 24};
            const ModalUtils::PopupSizing sizing({(std::min)(520.0f, maximum.x), (std::min)(420.0f, maximum.y)},
                {(std::min)(360.0f, maximum.x), (std::min)(240.0f, maximum.y)}, maximum, false);
            if (ImGui::BeginPopupModal(confirmTitle.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings)) {
                if (!GamepadInput::IsSteamKeyboardOpen() && ModalUtils::CancelPopupRequested()) ImGui::CloseCurrentPopup();
                const bool matches = state.reviewed && SameKitReview(*state.reviewed, preview);
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextDisabled("%s", matches ? localize("Workspace", "sGiveAllHint", "Items that fail are not retried.") : IssueText(KitIssue::Stale, view));
                ImGui::PopTextWrapPos();
                const float buttons = ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.y;
                if (ImGui::BeginChild("GiveKitItems", {0, (std::max)(1.0f, ImGui::GetContentRegionAvail().y - buttons)}, ImGuiChildFlags_Borders)) {
                    if (state.reviewed) for (const auto& action : state.reviewed->actions) {
                        const auto* record = view.catalog->Find(action.formID);
                        const auto name = !record ? std::string(localize("General", "sUnavailable", "Unavailable")) :
                            !record->name.empty() ? record->name : FormatUtils::FormID(record->formID);
                        ImGui::Text("%s x%u", name.c_str(), action.count);
                    }
                }
                ImGui::EndChild();
                bool firstButton = true;
                ImGui::BeginDisabled(!matches || state.submitted);
                if (ImGuiWidgetUtils::DrawWrappedButton(localize("Workspace", "sGive", "Give"), firstButton)) { requests.execute = *state.reviewed; ImGui::CloseCurrentPopup(); }
                ImGui::EndDisabled();
                if (ImGuiWidgetUtils::DrawWrappedButton(localize("General", "sCancel", "Cancel"), firstButton)) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }
}
