#include "GUI/Tabs/PluginBrowserHelpers.h"



#include "GUI/Widgets/BrowserWidgets.h"
#include "Core/RecordActions.h"
#include "GUI/Widgets/FormatUtils.h"


#include <algorithm>
#include <ranges>

namespace ESPExplorerAE::PluginBrowserHelpers
{
    namespace
    {
        bool IsDataCategory(std::string_view category)
        {
            return category == "KYWD" || category == "FLST" || category == "GLOB" || category == "COBJ";
        }

        const PluginInfo* FindPluginInfo(std::string_view pluginName, const std::vector<PluginInfo>& plugins)
        {
            const auto it = std::ranges::find_if(plugins, [pluginName](const PluginInfo& plugin) {
                return plugin.filename == pluginName;
            });
            return it != plugins.end() ? &(*it) : nullptr;
        }


    }

    std::string CopyLabel(const char* label, std::size_t count, const char* id)
    {
        return std::string(label) + " (" + std::to_string(count) + ")###" + id;
    }

    std::string BuildPluginDisplayName(std::string_view pluginName, const std::vector<PluginInfo>& plugins)
    {
        if (const auto* plugin = FindPluginInfo(pluginName, plugins); plugin && !plugin->formIDPrefix.empty()) {
            return std::string(pluginName) + " [" + plugin->formIDPrefix + "]";
        }

        return std::string(pluginName);
    }

    std::string CategoryDisplayName(std::string_view category, const Context& context)
    {
        if (category == "WEAP" || category == "Weapon") {
            return context.view.records.localize("Items", "sWeapons", "Weapons");
        }
        if (category == "ARMO" || category == "Armor") {
            return context.view.records.localize("Items", "sArmor", "Armor");
        }
        if (category == "AMMO" || category == "Ammo") {
            return context.view.records.localize("General", "sAmmunition", "Ammunition");
        }
        if (category == "ALCH") {
            return context.view.records.localize("General", "sAidChems", "Aid/Chems");
        }
        if (category == "BOOK") {
            return context.view.records.localize("General", "sBooks", "Books");
        }
        if (category == "MISC" || category == "Misc") {
            return context.view.records.localize("General", "sMiscellaneous", "Miscellaneous");
        }
        if (category == "KEYM") {
            return context.view.records.localize("General", "sKeys", "Keys");
        }
        if (category == "NOTE") {
            return context.view.records.localize("General", "sHolotapesNotes", "Holotapes/Notes");
        }
        if (category == "NPC" || category == "NPC_") {
            return context.view.records.localize("NPCs", "sTabName", "NPCs");
        }
        if (category == "LVLN") {
            return context.view.records.localize("General", "sLeveledNPCs", "Leveled NPCs");
        }
        if (category == "ACTI" || category == "Activator") {
            return context.view.records.localize("Objects", "sActivators", "Activators");
        }
        if (category == "CONT" || category == "Container") {
            return context.view.records.localize("Objects", "sContainers", "Containers");
        }
        if (category == "STAT" || category == "Static") {
            return context.view.records.localize("General", "sStaticObjects", "Static Objects");
        }
        if (category == "FURN" || category == "Furniture") {
            return context.view.records.localize("Objects", "sFurniture", "Furniture");
        }
        if (category == "SPEL" || category == "Spell") {
            return context.view.records.localize("Spells", "sSpells", "Spells");
        }
        if (category == "PERK" || category == "Perk") {
            return context.view.records.localize("Spells", "sPerks", "Perks");
        }
        if (category == "SNDR") {
            return context.view.records.localize("General", "sSoundDescriptors", "Sound Descriptors");
        }
        if (category == "SOUN") {
            return context.view.records.localize("General", "sSounds", "Sounds");
        }
        return std::string(category);
    }

    ImVec4 CategoryColor(std::string_view category)
    {
        if (SupportsRecordAction(category, ActionKind::Give)) {
            return ImVec4(0.40f, 0.80f, 0.40f, 1.00f);
        }
        if (SupportsRecordAction(category, ActionKind::Spawn)) {
            return ImVec4(0.82f, 0.62f, 0.38f, 1.00f);
        }
        if (category == "CELL" || category == "WRLD" || category == "LCTN" || category == "REGN") {
            return ImVec4(0.42f, 0.62f, 0.88f, 1.00f);
        }
        if (category == "WTHR") {
            return ImVec4(0.62f, 0.80f, 0.92f, 1.00f);
        }
        if (IsDataCategory(category)) {
            return ImVec4(0.80f, 0.72f, 0.52f, 1.00f);
        }
        return ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    }

    const FormEntry* FindRecordByFormID(const CatalogSnapshot& cache, std::uint32_t formID)
    {
        return cache.Find(formID);
    }

    void TrackRecentRecord(std::uint32_t formID, Context& context)
    {
        context.state.TrackRecent(formID, context.view.recentLimit);
    }

    void EnsurePrimarySelectionValid(Context& context)
    {
        if (context.state.selection.records.active != 0 && context.state.selection.records.selected.contains(context.state.selection.records.active)) {
            return;
        }

        if (context.state.selection.records.selected.empty()) {
            context.state.selection.records.active = 0;
            return;
        }

        context.state.selection.records.active = *std::ranges::min_element(context.state.selection.records.selected);
    }

    std::vector<RecordIndex> CollectSelectedEntries(const CatalogSnapshot& cache, const Context& context)
    {
        std::vector<RecordIndex> selected;
        selected.reserve(context.state.selection.records.selected.size());
        for (const auto index : context.state.query.Result().records.order) {
            if (context.state.selection.records.selected.contains(cache.records[index].formID)) selected.push_back(index);
        }
        return selected;
    }

    std::vector<RecordIndex> CollectSelectedGiveableEntries(const CatalogSnapshot& cache, const Context& context)
    {
        auto selected = CollectSelectedEntries(cache, context);
        std::erase_if(selected, [&](RecordIndex index) {
            const auto& record = cache.records[index];
            return record.isDeleted || !SupportsRecordAction(record.category, ActionKind::Give);
        });
        return selected;
    }

    void RequestGrant(std::span<const RecordIndex> records, Context& context)
    {
        if (records.empty()) return;
        BrowserRequests::Grant grant{ context.view.records.session, {} };
        grant.forms.reserve(records.size());
        for (const auto index : records) grant.forms.push_back(context.view.records.catalog->records[index].formID);
        context.requests.records.grants.push_back(std::move(grant));
    }

    void DrawRecordContextMenu(const FormEntry& record, bool isSelected, const CatalogSnapshot& cache, Context& context)
    {
        if (ImGui::MenuItem(isSelected ? context.view.records.localize("General", "sDeselect", "Deselect") : context.view.records.localize("General", "sSelect", "Select"))) {
            if (isSelected) {
                context.state.selection.records.selected.erase(record.formID);
            } else {
                context.state.selection.records.selected.insert(record.formID);
                context.state.selection.records.active = record.formID;
            }
            EnsurePrimarySelectionValid(context);
        }

        ImGui::Separator();

        const bool isMultiSelectedRecord = isSelected && context.state.selection.records.selected.size() > 1;
        if (isMultiSelectedRecord) {
            const auto selectedGiveableEntries = CollectSelectedGiveableEntries(cache, context);
            if (!selectedGiveableEntries.empty()) {
                std::string grantLabel = std::string(context.view.records.localize("Items", "sGiveItem", "Give Item")) + " (" + std::to_string(selectedGiveableEntries.size()) + ")###GiveSelection";
                if (ImGui::MenuItem(grantLabel.c_str(), nullptr, false, context.view.records.gameplayReady)) {
                    RequestGrant(selectedGiveableEntries, context);
                }
            }

            const auto selectedIndices = CollectSelectedEntries(cache, context);
            const auto selectedEntries = selectedIndices | std::views::transform([&](RecordIndex index) -> const FormEntry& { return cache.records[index]; });
            if (!selectedEntries.empty()) {
                if (ImGui::MenuItem(CopyLabel(context.view.records.localize("General", "sCopyFormID", "Copy FormID"), selectedEntries.size(), "sCopyFormID").c_str())) {
                    std::vector<std::string> values{};
                    values.reserve(selectedEntries.size());
                    for (const auto& selectedEntry : selectedEntries) {
                        values.push_back(FormatUtils::FormID(selectedEntry.formID));
                    }
                    const auto text = FormatUtils::MultiCopyList(values, context.view.copyFormat);
                    ImGui::SetClipboardText(text.c_str());
                }

                if (ImGui::MenuItem(CopyLabel(context.view.records.localize("General", "sCopyName", "Copy Name"), selectedEntries.size(), "sCopyName").c_str())) {
                    std::vector<std::string> values{};
                    values.reserve(selectedEntries.size());
                    for (const auto& selectedEntry : selectedEntries) {
                        values.push_back(selectedEntry.name.empty() ? context.view.records.localize("General", "sUnnamed", "<Unnamed>") : selectedEntry.name);
                    }
                    const auto text = FormatUtils::MultiCopyList(values, context.view.copyFormat);
                    ImGui::SetClipboardText(text.c_str());
                }

                if (ImGui::MenuItem(CopyLabel(context.view.records.localize("General", "sAddFavorite", "Add Favorite"), selectedEntries.size(), "sAddFavorite").c_str())) {
                    for (const auto& selectedEntry : selectedEntries) {
                        context.view.records.favorites.insert(selectedEntry.formID);
                    }
                }

                if (ImGui::MenuItem(CopyLabel(context.view.records.localize("General", "sRemoveFavorite", "Remove Favorite"), selectedEntries.size(), "sRemoveFavorite").c_str())) {
                    for (const auto& selectedEntry : selectedEntries) {
                        context.view.records.favorites.erase(selectedEntry.formID);
                    }
                }
            }

            ImGui::Separator();
        }

        BrowserWidgets::DrawContext(record, isMultiSelectedRecord ? BrowserWidgets::ContextScope::ActiveInSelection : BrowserWidgets::ContextScope::Single,
            context.state.contextQuantities, context.view.records, context.requests.records);
    }

}
