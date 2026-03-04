#include "GUI/Widgets/ItemGrantPopup.h"

#include "GUI/Widgets/FormActions.h"
#include "Localization/Language.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <unordered_set>

namespace ESPExplorerAE
{
    ItemGrantPopup::State& ItemGrantPopup::GetState()
    {
        static State state{};
        return state;
    }

    void ItemGrantPopup::Open(const FormEntry& entry)
    {
        Open(std::vector<FormEntry>{ entry });
    }

    void ItemGrantPopup::Open(const std::vector<FormEntry>& entries)
    {
        auto& state = GetState();
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
            itemState.quantity = 1;
            itemState.ammoQuantity = 100;
            itemState.ammoFormID = FormActions::GetWeaponAmmoFormID(entry.formID);
            itemState.includeAmmo = (entry.category == "Weapon" || entry.category == "Weapons" || entry.category == "WEAP") && itemState.ammoFormID != 0;
            state.items.push_back(std::move(itemState));
        }

        if (state.items.empty()) {
            state.visible = false;
            state.openRequested = false;
            return;
        }

        state.openRequested = true;
        state.visible = true;
    }

    void ItemGrantPopup::Draw(const LocalizeFn& localize)
    {
        auto& state = GetState();

        const auto L = [&](std::string_view section, std::string_view key, const char* fallback) -> const char* {
            if (localize) {
                return localize(section, key, fallback);
            }
            const auto value = Language::Get(section, key);
            return value.empty() ? fallback : value.data();
        };

        const bool multipleItems = state.items.size() > 1;
        const char* popupTitle = multipleItems ? L("Items", "sGivePopupTitleMulti", "Add Selected Items") : L("Items", "sGivePopupTitle", "Add Item");

        if (state.openRequested) {
            ImGui::OpenPopup(popupTitle);
            state.openRequested = false;
        }

        constexpr float kPopupWidth = 420.0f;
        ImGui::SetNextWindowSize(ImVec2(kPopupWidth, 0.0f), ImGuiCond_Always);
        if (!ImGui::BeginPopupModal(popupTitle, &state.visible, ImGuiWindowFlags_NoResize)) {
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            state.visible = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        const auto& style = ImGui::GetStyle();

        const float contentHeight = multipleItems ? 380.0f : 260.0f;
        if (ImGui::BeginChild("ItemGrantPopupItems", ImVec2(0.0f, contentHeight), false, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            if (multipleItems) {
                ImGui::TextDisabled("%zu %s", state.items.size(), L("General", "sItemsSelected", "items selected"));
                ImGui::Spacing();
            }

            for (std::size_t i = 0; i < state.items.size(); ++i) {
                auto& item = state.items[i];

                if (i > 0) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                }

                if (multipleItems) {
                    std::string header = std::to_string(i + 1) + ". " + (item.entry.name.empty() ? L("General", "sUnnamed", "<Unnamed>") : item.entry.name);
                    ImGui::TextUnformatted(header.c_str());
                } else {
                    ImGui::TextUnformatted(item.entry.name.empty() ? L("General", "sUnnamed", "<Unnamed>") : item.entry.name.c_str());
                }
                ImGui::TextDisabled("%08X  |  %s", item.entry.formID, item.entry.sourcePlugin.c_str());
                ImGui::Spacing();

                ImGui::TextUnformatted(L("Items", "sQuantity", "Quantity"));
                ImGui::Spacing();
                std::string itemQtySliderID = "##ItemQtySlider" + std::to_string(i);
                std::string itemQtyInputID = "##ItemQtyInput" + std::to_string(i);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::SliderInt(itemQtySliderID.c_str(), &item.quantity, 1, 100, "%d");
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputInt(itemQtyInputID.c_str(), &item.quantity, 1, 10);
                if (item.quantity < 1) {
                    item.quantity = 1;
                }
                if (item.quantity > 9999) {
                    item.quantity = 9999;
                }

                ImGui::Spacing();

                const char* presetLabels[] = { "1", "10", "100", "1000" };
                const int presetValues[] = { 1, 10, 100, 1000 };
                constexpr int presetCount = 4;
                const float availableWidth = ImGui::GetContentRegionAvail().x;
                const float totalSpacing = style.ItemSpacing.x * static_cast<float>(presetCount - 1);
                const float presetButtonWidth = (availableWidth - totalSpacing) / static_cast<float>(presetCount);

                for (int presetIndex = 0; presetIndex < presetCount; ++presetIndex) {
                    if (presetIndex > 0) {
                        ImGui::SameLine();
                    }
                    std::string presetID = std::string(presetLabels[presetIndex]) + "##ItemQtyPreset" + std::to_string(i);
                    if (ImGui::Button(presetID.c_str(), ImVec2(presetButtonWidth, 0.0f))) {
                        item.quantity = presetValues[presetIndex];
                    }
                }

                const bool hasAmmoOption = item.ammoFormID != 0;
                if (hasAmmoOption) {
                    ImGui::Spacing();
                    std::string includeAmmoID = std::string(L("Player", "sIncludeAmmo", "Include Ammo")) + "##IncludeAmmo" + std::to_string(i);
                    ImGui::Checkbox(includeAmmoID.c_str(), &item.includeAmmo);
                }

                if (hasAmmoOption && item.includeAmmo) {
                    ImGui::Spacing();
                    ImGui::TextUnformatted(L("Items", "sAmmoQuantity", "Ammo Quantity"));
                    ImGui::Spacing();
                    std::string ammoQtySliderID = "##AmmoQtySlider" + std::to_string(i);
                    std::string ammoQtyInputID = "##AmmoQtyInput" + std::to_string(i);
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::SliderInt(ammoQtySliderID.c_str(), &item.ammoQuantity, 0, 500, "%d");
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputInt(ammoQtyInputID.c_str(), &item.ammoQuantity, 10, 100);
                    if (item.ammoQuantity < 0) {
                        item.ammoQuantity = 0;
                    }
                    if (item.ammoQuantity > 50000) {
                        item.ammoQuantity = 50000;
                    }

                    ImGui::Spacing();

                    const char* ammoPresetLabels[] = { "0", "50", "100" };
                    const int ammoPresetValues[] = { 0, 50, 100 };
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
                            item.ammoQuantity = ammoPresetValues[ammoPresetIndex];
                        }
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const char* giveLabel = L("Items", "sGiveToPlayer", "Give To Player");
        const char* cancelLabel = L("General", "sCancel", "Cancel");
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
        if (!canApply) {
            ImGui::BeginDisabled(true);
        }
        const bool applyPressed = ImGui::Button(giveLabel, ImVec2(giveButtonWidth, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Enter);
        if (!canApply) {
            ImGui::EndDisabled();
        }
        if (applyPressed && canApply) {
            for (const auto& item : state.items) {
                if (item.quantity <= 0) {
                    continue;
                }

                if (item.includeAmmo && item.ammoFormID != 0 && item.ammoQuantity > 0) {
                    FormActions::GiveToPlayerWithAmmo(
                        item.entry.formID,
                        static_cast<std::uint32_t>(item.quantity),
                        item.ammoFormID,
                        static_cast<std::uint32_t>(item.ammoQuantity));
                } else {
                    FormActions::GiveToPlayer(item.entry.formID, static_cast<std::uint32_t>(item.quantity));
                }
            }
            state.visible = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button(cancelLabel, ImVec2(cancelButtonWidth, 0.0f))) {
            state.visible = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::Spacing();
        ImGui::EndPopup();
    }
}
