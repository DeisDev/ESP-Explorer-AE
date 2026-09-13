#include "GUI/Widgets/ItemGrantPopup.h"

#include "GUI/Widgets/FormatUtils.h"
#include "GUI/Widgets/ActionFeedback.h"
#include "GUI/Widgets/ImGuiWidgetUtils.h"
#include "GUI/Widgets/ModalUtils.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <unordered_set>

namespace ESPExplorerAE
{
    void ItemGrantPopup::Open(const FormEntry& entry, std::uint64_t session, bool includeAmmo, int ammoQuantity)
    {
        Open(std::vector<FormEntry>{ entry }, session, includeAmmo, ammoQuantity);
    }

    void ItemGrantPopup::Close()
    {
        ++state.revision;
        state.openRequested = state.visible = state.pending = false;
        state.closeRequested = true;
        state.items.clear();
    }

    void ItemGrantPopup::Open(const std::vector<FormEntry>& entries, std::uint64_t session, bool includeAmmo, int ammoQuantity)
    {
        ++state.revision;
        state.closeRequested = state.pending = false;
        state.session = session;
        state.admission = ActionAdmission::Accepted;
        state.items.clear();
        state.items.reserve(entries.size());

        std::unordered_set<std::uint32_t> seenFormIDs{};
        seenFormIDs.reserve(entries.size());

        for (const auto& entry : entries) {
            if (entry.formID == 0 || seenFormIDs.contains(entry.formID)) {
                continue;
            }

            seenFormIDs.insert(entry.formID);

            State::ItemState itemState{};
            itemState.entry = entry;
            itemState.ammoQuantity = (std::clamp)(ammoQuantity, 0, 50000);
            itemState.ammoFormID = entry.weaponAmmoID;
            itemState.includeAmmo = includeAmmo && (entry.category == "Weapon" || entry.category == "Weapons" || entry.category == "WEAP") && itemState.ammoFormID != 0;
            state.items.push_back(std::move(itemState));
        }

        if (state.items.empty()) {
            Close();
            return;
        }

        state.openRequested = true;
        state.visible = true;
    }

    void ItemGrantPopup::ResolveSubmit(std::uint64_t revision, ActionAdmission admission)
    {
        if (!state.pending || state.revision != revision) return;
        state.pending = false;
        state.admission = admission;
        if (admission == ActionAdmission::Accepted) Close();
    }

    void ItemGrantPopup::Draw(const View& view, Requests& requests)
    {
        if (state.session != view.session && Visible()) Close();
        if (state.closeRequested) {
            if (ImGui::BeginPopupModal("###ItemGrantPopup")) {
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            state.closeRequested = false;
            return;
        }
        if (state.items.empty() || (!state.visible && !state.openRequested)) return;

        const auto L = [&](std::string_view section, std::string_view key, const char* fallback) -> const char* {
            return view.localize ? view.localize(section, key, fallback) : fallback;
        };

        const bool multipleItems = state.items.size() > 1;
        const auto popupTitle = std::string(multipleItems ? L("Items", "sGivePopupTitleMulti", "Add Selected Items") : L("Items", "sGivePopupTitle", "Add Item")) + "###ItemGrantPopup";

        if (state.openRequested) {
            if (!ModalUtils::CanOpenPopup(popupTitle.c_str())) return;
            ImGui::OpenPopup(popupTitle.c_str());
            state.openRequested = false;
        }

        const float popupScale = (std::clamp)(view.fontSize / 20.0f, 0.75f, 1.5f);
        const float popupWidth = (std::clamp)(520.0f * popupScale, 420.0f, 900.0f);
        const bool singleItemHasAmmoControls = !multipleItems && !state.items.empty() && state.items.front().ammoFormID != 0 && state.items.front().includeAmmo;
        const float popupHeight = multipleItems ?
                                      (std::clamp)(620.0f * popupScale, 420.0f, 980.0f) :
                                      (std::clamp)((singleItemHasAmmoControls ? 560.0f : 480.0f) * popupScale, 400.0f, 860.0f);
        const auto* viewport = ImGui::GetMainViewport();
        const ImVec2 maxSize((std::min)(popupWidth * 1.60f, viewport->WorkSize.x - 32.0f), (std::min)(popupHeight * 1.60f, viewport->WorkSize.y - 32.0f));
        const ImVec2 minSize((std::min)(420.0f, maxSize.x), (std::min)(400.0f, maxSize.y));
        ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        const ModalUtils::PopupSizing popupSizing(ImVec2((std::min)(popupWidth, maxSize.x), (std::min)(popupHeight, maxSize.y)), minSize, maxSize, false);
        if (!ImGui::BeginPopupModal(popupTitle.c_str(), &state.visible)) {
            if (!state.visible) Close();
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            Close();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        const auto& style = ImGui::GetStyle();
        ImGuiWidgetUtils::DrawStatusArea("##GrantFeedback", [&] { ActionFeedback::Draw(state.admission, L); });

        const auto drawQuantityButtons = [&](const char* id, auto&& apply) {
            ImGui::PushID(id);
            const char* labels[]{ "+1", "+10", "+100", "+1000" };
            const int amounts[]{ 1, 10, 100, 1000 };
            const float width = (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 3.0f) / 4.0f;
            for (int preset = 0; preset < 4; ++preset) {
                if (preset) ImGui::SameLine();
                if (ImGui::Button(labels[preset], ImVec2(width, 0))) apply(amounts[preset]);
            }
            if (ImGui::Button(L("Items", "sClearQuantity", "Clear Quantity"), ImVec2(-1, 0))) apply(0);
            ImGui::PopID();
        };

        ImGui::BeginDisabled(state.pending);
        if (multipleItems) {
            ImGui::TextWrapped("%s", L("Items", "sAllSelectedQuantities", "Add to every selected item"));
            drawQuantityButtons("AllQuantities", [&](int amount) {
                for (auto& item : state.items) item.quantity = amount ? (std::min)(item.quantity + amount, 9999) : 0;
            });
            ImGui::Spacing();
        }

        auto drawItemControls = [&](std::size_t i, State::ItemState& item) {
            if (multipleItems) {
                std::string header = std::to_string(i + 1) + ". " + (item.entry.name.empty() ? L("General", "sUnnamed", "<Unnamed>") : item.entry.name);
                ImGui::TextUnformatted(header.c_str());
            } else {
                ImGui::TextUnformatted(item.entry.name.empty() ? L("General", "sUnnamed", "<Unnamed>") : item.entry.name.c_str());
            }
            const std::string formIDText = FormatUtils::FormID(item.entry.formID);
            ImGui::TextDisabled("%s  |  %s", formIDText.c_str(), item.entry.sourcePlugin.c_str());
            ImGui::Spacing();

            ImGui::TextUnformatted(L("Items", "sQuantity", "Quantity"));
            std::string itemQtyInputID = "##ItemQtyInput" + std::to_string(i);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputInt(itemQtyInputID.c_str(), &item.quantity, 1, 10);
            item.quantity = (std::clamp)(item.quantity, 0, 9999);

            ImGui::Spacing();

            drawQuantityButtons(("ItemQuantity" + std::to_string(i)).c_str(), [&](int amount) {
                item.quantity = amount ? (std::min)(item.quantity + amount, 9999) : 0;
            });

            const bool hasAmmoOption = item.ammoFormID != 0;
            if (hasAmmoOption) {
                ImGui::Spacing();
                std::string includeAmmoID = std::string(L("Inventory", "sIncludeAmmo", "Include Ammo")) + "##IncludeAmmo" + std::to_string(i);
                ImGui::Checkbox(includeAmmoID.c_str(), &item.includeAmmo);
            }

            if (hasAmmoOption && item.includeAmmo) {
                ImGui::Spacing();
                ImGui::TextUnformatted(L("Items", "sAmmoQuantity", "Ammo Quantity"));
                std::string ammoQtyInputID = "##AmmoQtyInput" + std::to_string(i);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputInt(ammoQtyInputID.c_str(), &item.ammoQuantity, 10, 100);
                if (item.ammoQuantity < 0) {
                    item.ammoQuantity = 0;
                }
                if (item.ammoQuantity > 50000) {
                    item.ammoQuantity = 50000;
                }

                ImGui::Spacing();

                const char* ammoPresetLabels[] = { "+50", "+100", "+1000" };
                const int ammoPresetValues[] = { 50, 100, 1000 };
                constexpr int ammoPresetCount = 3;
                const float ammoAvailableWidth = ImGui::GetContentRegionAvail().x;
                const float ammoTotalSpacing = style.ItemSpacing.x * static_cast<float>(ammoPresetCount - 1);
                const float ammoPresetWidth = (ammoAvailableWidth - ammoTotalSpacing) / static_cast<float>(ammoPresetCount);

                for (int ammoPresetIndex = 0; ammoPresetIndex < ammoPresetCount; ++ammoPresetIndex) {
                    if (ammoPresetIndex > 0) {
                        ImGui::SameLine();
                    }
                    std::string ammoPresetID = std::string(ammoPresetLabels[ammoPresetIndex]) + "##AmmoQtyPreset" + std::to_string(i);
                    if (ImGui::Button(ammoPresetID.c_str(), ImVec2(ammoPresetWidth, 0.0f))) {
                        item.ammoQuantity = (std::min)(item.ammoQuantity + ammoPresetValues[ammoPresetIndex], 50000);
                    }
                }
                if (ImGui::Button((std::string(L("Items", "sClearQuantity", "Clear Quantity")) + "##ClearAmmo" + std::to_string(i)).c_str(), ImVec2(-1, 0))) {
                    item.ammoQuantity = 0;
                }
            }
        };

        // Keep totals and submission controls visible while long item lists scroll.
        const float footerReserve = ImGui::GetFrameHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing() * 2.0f + style.ItemSpacing.y * 4.0f;
        const float contentHeight = (std::max)(ImGui::GetFrameHeight(), ImGui::GetContentRegionAvail().y - footerReserve);
        if (ImGui::BeginChild("ItemGrantPopupItems", ImVec2(0.0f, contentHeight), false)) {
            if (multipleItems) {
                ImGui::TextDisabled("%zu %s", state.items.size(), L("General", "sItemsSelected", "items selected"));
                ImGui::Spacing();
            }
            for (std::size_t i = 0; i < state.items.size(); ++i) {
                if (i > 0) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                }
                drawItemControls(i, state.items[i]);
            }
        }
        ImGui::EndChild();
        ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        std::uint64_t totalItems{}, totalAmmo{};
        for (const auto& item : state.items) {
            totalItems += item.quantity;
            if (item.quantity > 0 && item.includeAmmo && item.ammoFormID) totalAmmo += item.ammoQuantity;
        }
        ImGui::Text("%s: %llu", L("Items", "sTotalItems", "Total items"), static_cast<unsigned long long>(totalItems));
        ImGui::Text("%s: %llu", L("Items", "sExtraAmmo", "Extra ammo"), static_cast<unsigned long long>(totalAmmo));
        const char* giveLabel = L("Items", "sGiveToPlayer", "Give To Player");
        const char* cancelLabel = L("General", "sCancel", "Cancel");
        const bool gameplayActionsAllowed = view.gameplayReady;
        const char* disabledTooltip = L("General", "sGameplayActionsDisabledInMainMenu", "Gameplay actions are disabled while the main menu is open.");
        const float giveTextWidth = ImGui::CalcTextSize(giveLabel).x + style.FramePadding.x * 2.0f;
        const float cancelTextWidth = ImGui::CalcTextSize(cancelLabel).x + style.FramePadding.x * 2.0f;
        const float minButtonWidth = 120.0f;
        const float giveButtonWidth = (std::max)(giveTextWidth, minButtonWidth);
        const float cancelButtonWidth = (std::max)(cancelTextWidth, minButtonWidth);
        const float totalButtonsWidth = giveButtonWidth + cancelButtonWidth + style.ItemSpacing.x;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - totalButtonsWidth) * 0.5f + style.WindowPadding.x);

        bool canApply = false;
        for (const auto& item : state.items) {
            if (item.quantity > 0) {
                canApply = true;
                break;
            }
        }
        canApply = canApply && gameplayActionsAllowed && !state.pending;
        if (!canApply) {
            ImGui::BeginDisabled(true);
        }
        const bool applyPressed = ImGui::Button(giveLabel, ImVec2(giveButtonWidth, 0.0f)) ||
            (!ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() && ImGui::IsKeyPressed(ImGuiKey_Enter));
        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
        if (!totalItems && gameplayActionsAllowed && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("%s", L("Items", "sChooseQuantity", "Choose a quantity to add. Zero quantities are skipped."));
        }
        if (!canApply) {
            ImGui::EndDisabled();
        }
        if (applyPressed && canApply) {
            std::vector<ActionRequest> batch;
            batch.reserve(state.items.size());
            for (const auto& item : state.items) {
                if (item.quantity <= 0) {
                    continue;
                }

                if (item.includeAmmo && item.ammoFormID != 0 && item.ammoQuantity > 0) {
                    batch.push_back({ .kind = ActionKind::GiveWithAmmo, .formID = item.entry.formID,
                        .count = static_cast<std::uint32_t>(item.quantity), .ammoFormID = item.ammoFormID,
                        .ammoCount = static_cast<std::uint32_t>(item.ammoQuantity), .session = state.session });
                } else {
                    batch.push_back({ .kind = ActionKind::Give, .formID = item.entry.formID,
                        .count = static_cast<std::uint32_t>(item.quantity), .session = state.session });
                }
            }
            state.pending = true;
            requests.submit = Submission{ ++state.revision, std::move(batch) };
        }

        ImGui::SameLine();
        if (ImGui::Button(cancelLabel, ImVec2(cancelButtonWidth, 0.0f))) {
            Close();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
