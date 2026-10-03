#include "GUI/Widgets/WorkspaceView.h"
#include "GUI/Widgets/ModalUtils.h"
#include "GUI/Widgets/SearchBar.h"
#include "GUI/Widgets/ImGuiWidgetUtils.h"
#include "Input/GamepadInput.h"
#include <imgui.h>

namespace ESPExplorerAE
{
    namespace
    {
        enum class EntryMenu { None, Renamed, Delete };

        bool BeginEntryContext()
        {
            if (ImGui::IsItemFocused() && !ImGui::IsAnyItemActive() && !GamepadInput::IsSteamKeyboardOpen() &&
                ((ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_F10, false)) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft, false)))
                ImGui::OpenPopup("EntryContext");
            return ImGui::BeginPopupContextItem("EntryContext");
        }

        bool SourceGroup(const char* id, const char* label)
        {
            const float labelX = ImGui::GetCursorPosX() + ImGui::GetTreeNodeToLabelSpacing();
            const bool open = ImGui::TreeNodeEx(id, ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen, "%s", "");
            ImGui::SameLine(labelX);
            ImGui::TextWrapped("%s", label);
            return open;
        }

        void DisabledWrapped(const char* text)
        {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", text);
            ImGui::PopTextWrapPos();
        }

        WorkspaceDocument& Edit(WorkspaceViewRequests& requests, const WorkspaceDocument& document)
        {
            if (!requests.update) requests.update = document;
            return *requests.update;
        }

        template<class T> bool ValidName(const std::vector<T>& groups, std::string_view name, std::size_t except = static_cast<std::size_t>(-1))
        {
            if (name.empty() || name.size() > 128) return false;
            for (std::size_t i = 0; i < groups.size(); ++i) if (i != except && FoldSearchText(groups[i].name) == FoldSearchText(name)) return false;
            return true;
        }
        template<class T> void AppendGroups(std::vector<T>& target, std::vector<T> source)
        {
            for (auto& group : source) {
                const auto base = group.name;
                for (int suffix = 2; !ValidName(target, group.name); ++suffix) group.name = base.substr(0, 110) + " (" + std::to_string(suffix) + ")";
                target.push_back(std::move(group));
            }
        }

        void AddRecords(RecordCollection& collection, const std::vector<std::uint32_t>& ids, const BrowserView& view)
        {
            for (auto id : ids) {
                auto record = CaptureWorkspaceRecord(*view.catalog, id, view.session);
                if (std::ranges::none_of(collection.records, [&](const auto& existing) {
                    return record.identity.empty() ? existing.identity.empty() && existing.transientID == id && existing.session == view.session : existing.identity == record.identity;
                })) collection.records.push_back(std::move(record));
            }
        }

        bool NameInput(const char* id, std::array<char, 129>& buffer, const char* hint, float width)
        {
            ImGui::SetNextItemWidth(width);
            const bool enter = ImGui::InputTextWithHint(id, hint, buffer.data(), buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue);
            SearchBar::ReadControllerText(hint, buffer.data(), buffer.size());
            return enter;
        }

        // A context menu that renames in place and offers Delete.
        template<class T> EntryMenu NameMenu(WorkspaceViewState& state, const std::vector<T>& groups, std::size_t index, bool writable, const BrowserView& view)
        {
            const auto& localize = view.localize;
            if (ImGui::IsWindowAppearing()) std::snprintf(state.rename.data(), state.rename.size(), "%s", groups[index].name.c_str());
            ImGui::BeginDisabled(!writable);
            const bool enter = NameInput("##Rename", state.rename, localize("General", "sName", "Name"), ImGui::GetFontSize() * 14.0f);
            const bool valid = ValidName(groups, state.rename.data(), index) && groups[index].name != state.rename.data();
            ImGui::SameLine();
            ImGui::BeginDisabled(!valid);
            const bool rename = ImGui::Button(localize("Workspace", "sRename", "Rename")) || (enter && valid);
            ImGui::EndDisabled();
            ImGui::Separator();
            const bool remove = ImGui::MenuItem(localize("Workspace", "sDelete", "Delete"));
            ImGui::EndDisabled();
            if (rename) { ImGui::CloseCurrentPopup(); return EntryMenu::Renamed; }
            return remove ? EntryMenu::Delete : EntryMenu::None;
        }

        void CollectionMenu(WorkspaceViewState& state, const WorkspaceDocument& document, std::size_t index, bool writable,
            const BrowserView& view, WorkspaceViewRequests& requests)
        {
            switch (NameMenu(state, document.collections, index, writable, view)) {
            case EntryMenu::Renamed: {
                auto& edit = Edit(requests, document);
                const auto previous = edit.collections[index].name;
                const std::string name = state.rename.data();
                for (auto& saved : edit.views) if (saved.location.query.scope.collection == previous) saved.location.query.scope.collection = name;
                edit.collections[index].name = name;
                if (state.selectedCollection == previous) state.selectedCollection = name;
                requests.renamedCollection = {previous, name};
                break;
            }
            case EntryMenu::Delete:
                state.pendingDelete = document.collections[index].name;
                state.deleteRequested = true;
                break;
            default: break;
            }
        }

        void DeleteCollectionPopup(WorkspaceViewState& state, const WorkspaceDocument& document, bool writable,
            const BrowserView& view, WorkspaceViewRequests& requests)
        {
            const auto& localize = view.localize;
            if (state.deleteRequested) { ImGui::OpenPopup("DeleteCollection"); state.deleteRequested = false; }
            if (!ImGui::BeginPopup("DeleteCollection")) return;
            ImGuiWidgetUtils::PushPopupTextWrap();
            ImGui::TextUnformatted(state.pendingDelete.c_str());
            ImGui::TextWrapped("%s", localize("Workspace", "sDeleteCollectionConfirm", "Remove this collection and its notes? Game records are unaffected."));
            ImGui::PopTextWrapPos();
            bool first = true;
            if (ImGuiWidgetUtils::DrawWrappedButton(localize("General", "sCancel", "Cancel"), first)) ImGui::CloseCurrentPopup();
            ImGui::BeginDisabled(!writable);
            if (ImGuiWidgetUtils::DrawWrappedButton(localize("General", "sRemove", "Remove"), first)) {
                std::erase_if(Edit(requests, document).collections, [&](const auto& collection) { return collection.name == state.pendingDelete; });
                if (state.selectedCollection == state.pendingDelete) { state.selectedCollection.clear(); state.noteIndex = static_cast<std::size_t>(-1); }
                requests.deletedCollection = state.pendingDelete;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }

        void ImportPopup(WorkspaceViewState& state, const WorkspaceDocument& document, bool writable,
            const BrowserView& view, WorkspaceViewRequests& requests)
        {
            const auto& localize = view.localize;
            const auto title = std::string(localize("Workspace", "sImportTitle", "Import Workspace")) + "###ImportWorkspace";
            if (state.importRequested) { ImGui::OpenPopup(title.c_str()); state.importRequested = false; }
            const auto* viewport = ImGui::GetMainViewport();
            const ImVec2 maximum{viewport->WorkSize.x - 24, viewport->WorkSize.y - 24};
            const ModalUtils::PopupSizing sizing({(std::min)(560.0f, maximum.x), (std::min)(420.0f, maximum.y)},
                {(std::min)(360.0f, maximum.x), (std::min)(240.0f, maximum.y)}, maximum, false);
            if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings)) return;
            if (!GamepadInput::IsSteamKeyboardOpen() && ModalUtils::CancelPopupRequested()) { state.imported.reset(); ImGui::CloseCurrentPopup(); }
            const float buttons = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
            bool canImport = false;
            if (!state.imported || !*state.imported) DisabledWrapped(localize("Workspace", "sInvalidImport", "Import rejected: unsupported schema, invalid entries, or input too large. No changes were made."));
            else {
                canImport = writable;
                const auto& incoming = state.imported->document;
                ImGui::Text("%s: %zu | %s: %zu | %s: %zu", localize("Workspace", "sSavedViews", "Saved Views"), incoming.views.size(),
                    localize("Workspace", "sCollections", "Collections"), incoming.collections.size(), localize("Workspace", "sItemKits", "Item Kits"), incoming.kits.size());
                DisabledWrapped(localize("Workspace", "sImportHint", "Adds entries and renames duplicates. Missing records are kept; kits are saved without giving items."));
                FavoriteIdentity identity(*view.catalog);
                const auto preview = [&](const WorkspaceRecord& record) {
                    if (!ResolveWorkspaceRecord(identity, record, view.session) && (!record.identity.empty() || !record.name.empty()))
                        ImGui::BulletText("%s: %s | %s", localize("Workspace", "sUnresolved", "Unresolved; retained for later"), record.name.c_str(), record.identity.c_str());
                };
                if (ImGui::BeginChild("ImportMissing", {0, (std::max)(1.0f, ImGui::GetContentRegionAvail().y - buttons)}, ImGuiChildFlags_Borders)) {
                    for (const auto& collection : incoming.collections) for (const auto& record : collection.records) preview(record);
                    for (const auto& kit : incoming.kits) for (const auto& entry : kit.entries) preview(entry.record);
                    for (const auto& saved : incoming.views) {
                        for (const auto& record : saved.selected) preview(record);
                        for (const auto& clause : saved.clauses) if (clause.field == SearchField::ID) preview(clause.target);
                        for (const auto& plugin : saved.location.query.scope.plugins)
                            if (std::ranges::none_of(view.catalog->plugins, [&](const auto& loaded) { return SearchIdentityEquals(loaded.filename, plugin); }))
                                ImGui::BulletText("%s: %s", localize("Workspace", "sMissingPlugin", "Missing Plugin"), plugin.c_str());
                    }
                }
                ImGui::EndChild();
            }
            bool first = true;
            ImGui::BeginDisabled(!canImport);
            if (ImGuiWidgetUtils::DrawWrappedButton(localize("Workspace", "sImport", "Import"), first)) {
                auto merged = document;
                auto imported = state.imported->document;
                for (auto& collection : imported.collections) {
                    const auto previous = collection.name;
                    for (int suffix = 2; !ValidName(merged.collections, collection.name); ++suffix) collection.name = previous.substr(0, 110) + " (" + std::to_string(suffix) + ")";
                    for (auto& saved : imported.views) if (saved.location.query.scope.collection == previous) saved.location.query.scope.collection = collection.name;
                    merged.collections.push_back(std::move(collection));
                }
                AppendGroups(merged.views, std::move(imported.views));
                AppendGroups(merged.kits, std::move(imported.kits));
                requests.update = std::move(merged);
                state.imported.reset();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            if (ImGuiWidgetUtils::DrawWrappedButton(localize("General", "sCancel", "Cancel"), first)) { state.imported.reset(); ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }

        void DrawCollectionRecords(WorkspaceViewState& state, const WorkspaceDocument& document, std::size_t index, bool writable,
            const BrowserView& view, WorkspaceViewRequests& requests)
        {
            const auto& localize = view.localize;
            const auto& collection = document.collections[index];
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(collection.name.c_str());
            ImGuiWidgetUtils::DrawWrappedSameLine(localize("Workspace", "sSearchCollection", "Search This Collection"));
            if (ImGui::Button(localize("Workspace", "sSearchCollection", "Search This Collection"))) requests.searchCollection = collection.name;
            if (collection.records.empty()) {
                DisabledWrapped(localize("Workspace", "sNoCollectionRecords", "This collection is empty."));
                DisabledWrapped(localize("Workspace", "sCollectionsHint", "Right-click a record and choose Add to Collection."));
                return;
            }
            if (state.noteIndex >= collection.records.size()) state.noteIndex = static_cast<std::size_t>(-1);
            const bool editingNote = state.noteIndex < collection.records.size();
            const float noteHeight = editingNote ? ImGui::GetTextLineHeightWithSpacing() + ImGui::GetFrameHeight() * 2.5f + ImGui::GetStyle().ItemSpacing.y : 0.0f;
            FavoriteIdentity identity(*view.catalog);
            constexpr auto flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("CollectionRecords", 2, flags, {0, (std::max)(ImGui::GetFrameHeight() * 3.0f, ImGui::GetContentRegionAvail().y - noteHeight)})) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn(localize("General", "sName", "Name"), ImGuiTableColumnFlags_WidthStretch, 3.0f);
                ImGui::TableSetupColumn(localize("General", "sPlugin", "Plugin"), ImGuiTableColumnFlags_WidthStretch, 2.0f);
                ImGui::TableHeadersRow();
                for (std::size_t i = 0; i < collection.records.size(); ++i) {
                    const auto& record = collection.records[i];
                    const auto target = ResolveWorkspaceRecord(identity, record, view.session);
                    const auto* entry = target ? view.catalog->Find(target.formID) : nullptr;
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    const auto& name = !record.name.empty() ? record.name : record.identity;
                    if (ImGui::Selectable(name.empty() ? localize("General", "sUnnamed", "<Unnamed>") : name.c_str(), state.noteIndex == i, ImGuiSelectableFlags_SpanAllColumns)) {
                        state.noteIndex = i;
                        std::snprintf(state.note.data(), state.note.size(), "%s", record.note.c_str());
                        if (target) requests.inspect.push_back(target.formID);
                    }
                    if (BeginEntryContext()) {
                        if (ImGui::MenuItem(localize("General", "sRemove", "Remove"), nullptr, false, writable)) {
                            auto& records = Edit(requests, document).collections[index].records;
                            records.erase(records.begin() + static_cast<std::ptrdiff_t>(i));
                            state.noteIndex = static_cast<std::size_t>(-1);
                        }
                        ImGui::EndPopup();
                    }
                    if (!record.note.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", record.note.c_str());
                    ImGui::TableNextColumn();
                    if (entry) ImGui::TextUnformatted(entry->sourcePlugin.c_str());
                    else ImGui::TextDisabled("%s", record.identity.empty() ? localize("Workspace", "sSessionOnly", "Session only; not restored as a target") :
                        localize("Workspace", "sUnresolved", "Unresolved; retained for later"));
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            if (state.noteIndex < collection.records.size()) {
                ImGui::TextDisabled("%s", localize("Workspace", "sNote", "Note"));
                ImGui::BeginDisabled(!writable);
                bool edited = ImGui::InputTextMultiline("##Note", state.note.data(), state.note.size(), {-FLT_MIN, ImGui::GetFrameHeight() * 2.5f});
                edited |= SearchBar::ReadControllerText(localize("Workspace", "sNote", "Note"), state.note.data(), state.note.size());
                if (edited) Edit(requests, document).collections[index].records[state.noteIndex].note = state.note.data();
                ImGui::EndDisabled();
            }
        }
    }

    void DrawWorkspaceSources(WorkspaceViewState& state, const WorkspaceDocument& document, RecordScope& scope,
        const BrowserView& view, bool writable, WorkspaceViewRequests& requests)
    {
        const auto& localize = view.localize;
        const auto label = std::string(localize("Workspace", "sCollections", "Collections")) + " (" + std::to_string(document.collections.size()) + ")";
        if (SourceGroup("SourceCollections", label.c_str())) {
            if (document.collections.empty()) {
                DisabledWrapped(localize("Workspace", "sNoCollections", "No collections yet."));
                DisabledWrapped(localize("Workspace", "sCollectionsHint", "Right-click a record and choose Add to Collection."));
            }
            for (std::size_t i = 0; i < document.collections.size(); ++i) {
                const auto& collection = document.collections[i];
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(collection.name.c_str(), scope.kind == SearchScope::Collection && scope.collection == collection.name)) {
                    scope.kind = SearchScope::Collection; scope.collection = collection.name;
                    ResolveCollectionScope(scope, document, *view.catalog, view.session);
                }
                if (BeginEntryContext()) { CollectionMenu(state, document, i, writable, view, requests); ImGui::EndPopup(); }
                ImGui::PopID();
            }
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            if (ImGui::Selectable(localize("Workspace", "sManageCollections", "Manage Collections"))) requests.manage = true;
            ImGui::PopStyleColor();
            ImGui::TreePop();
        }
        DeleteCollectionPopup(state, document, writable, view, requests);
    }

    void DrawSavedViewsMenu(WorkspaceViewState& state, const WorkspaceDocument& document, const WorkspaceLocation& current,
        const BrowserView& view, bool writable, WorkspaceViewRequests& requests)
    {
        if (!ImGui::BeginPopup("SavedViewsMenu")) return;
        const auto& localize = view.localize;
        if (ImGui::IsWindowAppearing()) state.name[0] = '\0';
        const bool validView = DestinationFor(current.page) == WorkspaceDestination::Explore && (!current.query.structuredSearch || ParseSearch(current.query.search));
        const bool enter = NameInput("##ViewName", state.name, localize("General", "sName", "Name"), ImGui::GetFontSize() * 14.0f);
        const bool canSave = writable && validView && ValidName(document.views, state.name.data());
        ImGui::SameLine();
        ImGui::BeginDisabled(!canSave);
        if (ImGui::Button(localize("Workspace", "sSaveView", "Save Current View")) || (enter && canSave)) {
            Edit(requests, document).views.push_back(CaptureSavedView(state.name.data(), current, *view.catalog, view.session));
            state.name[0] = '\0';
        }
        ImGui::EndDisabled();
        if (!writable || state.failed) {
            ImGuiWidgetUtils::PushPopupTextWrap();
            ImGui::TextDisabled("%s", !writable ? localize("Workspace", "sReadOnly", "Workspace could not be loaded. Saving is disabled; you can still export.") :
                localize("Workspace", "sSaveFailed", "Changes could not be saved. Check names, input limits, and workspace file access."));
            ImGui::PopTextWrapPos();
        }
        ImGui::Separator();
        if (document.views.empty()) ImGui::TextDisabled("%s", localize("Workspace", "sNoViews", "No saved views yet."));
        for (std::size_t i = 0; i < document.views.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable(document.views[i].name.c_str())) requests.restore = ResolveSavedView(document.views[i], *view.catalog, view.session);
            if (BeginEntryContext()) {
                switch (NameMenu(state, document.views, i, writable, view)) {
                case EntryMenu::Renamed: Edit(requests, document).views[i].name = state.rename.data(); break;
                case EntryMenu::Delete: { auto& views = Edit(requests, document).views; views.erase(views.begin() + static_cast<std::ptrdiff_t>(i)); break; }
                default: break;
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }

    void DrawCollectionsWindow(WorkspaceViewState& state, const WorkspaceDocument& document,
        const BrowserView& view, bool writable, WorkspaceViewRequests& requests)
    {
        if (!state.open) return;
        const auto& localize = view.localize;
        const std::string title = std::string(localize("Workspace", "sCollections", "Collections")) + "###WorkspaceManager";
        ModalUtils::PrepareToolWindow(title.c_str(), {760, 560}, {460, 320}, state.focusPending);
        if (ImGui::Begin(title.c_str(), &state.open, ImGuiWindowFlags_NoCollapse)) {
            if (ModalUtils::EscapeClosesCurrentWindow()) state.open = false;
            const float font = ImGui::GetFontSize();
            ImGuiWidgetUtils::DrawStatusArea("##WorkspaceFeedback", [&] {
                if (!writable) ImGui::TextWrapped("%s", localize("Workspace", "sReadOnly", "Workspace could not be loaded. Saving is disabled; you can still export."));
                if (state.failed || state.storageFailed) ImGui::TextWrapped("%s", localize("Workspace", "sSaveFailed", "Changes could not be saved. Check names, input limits, and workspace file access."));
            });
            bool first = true;
            ImGui::BeginDisabled(!writable);
            if (ImGuiWidgetUtils::DrawWrappedButton(localize("Workspace", "sNewCollection", "New Collection"), first)) ImGui::OpenPopup("NewCollection");
            ImGui::EndDisabled();
            if (ImGuiWidgetUtils::DrawWrappedButton(localize("Workspace", "sImportExport", "Import / Export"), first)) ImGui::OpenPopup("ImportExport");
            if (ImGui::BeginPopup("NewCollection")) {
                if (ImGui::IsWindowAppearing()) { state.name[0] = '\0'; ImGui::SetKeyboardFocusHere(); }
                const bool enter = NameInput("##CollectionName", state.name, localize("General", "sName", "Name"), font * 14.0f);
                const bool valid = ValidName(document.collections, state.name.data());
                ImGui::SameLine();
                ImGui::BeginDisabled(!valid);
                if (ImGui::Button(localize("Workspace", "sCreate", "Create")) || (enter && valid)) {
                    Edit(requests, document).collections.push_back({state.name.data(), {}});
                    state.selectedCollection = state.name.data();
                    state.noteIndex = static_cast<std::size_t>(-1);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
                ImGui::EndPopup();
            }
            if (ImGui::BeginPopup("ImportExport")) {
                if (ImGui::MenuItem(localize("Workspace", "sExportClipboard", "Export Workspace to Clipboard"))) {
                    std::string bytes;
                    if (WriteWorkspaceIni(document, bytes)) ImGui::SetClipboardText(bytes.c_str()); else state.failed = true;
                }
                if (ImGui::MenuItem(localize("Workspace", "sPreviewImport", "Import from Clipboard"), nullptr, false, writable)) {
                    const char* clipboard = ImGui::GetClipboardText();
                    std::size_t size{};
                    while (clipboard && size <= WorkspaceDocument::MaxBytes && clipboard[size]) ++size;
                    state.imported = clipboard && size <= WorkspaceDocument::MaxBytes ? ReadWorkspaceIni({clipboard, size}) : WorkspaceReadResult{{}, "Input too large"};
                    state.importRequested = true;
                }
                ImGui::EndPopup();
            }

            const float listWidth = (std::clamp)(font * 12.0f, 1.0f, (std::max)(1.0f, ImGui::GetContentRegionAvail().x * 0.4f));
            if (ImGui::BeginChild("CollectionList", {listWidth, 0}, ImGuiChildFlags_Borders)) {
                if (document.collections.empty()) {
                    DisabledWrapped(localize("Workspace", "sNoCollections", "No collections yet."));
                    DisabledWrapped(localize("Workspace", "sCollectionsHint", "Right-click a record and choose Add to Collection."));
                }
                for (std::size_t i = 0; i < document.collections.size(); ++i) {
                    const auto& collection = document.collections[i];
                    ImGui::PushID(static_cast<int>(i));
                    const auto label = collection.name + " (" + std::to_string(collection.records.size()) + ")";
                    if (ImGui::Selectable(label.c_str(), state.selectedCollection == collection.name)) {
                        state.selectedCollection = collection.name;
                        state.noteIndex = static_cast<std::size_t>(-1);
                    }
                    if (BeginEntryContext()) { CollectionMenu(state, document, i, writable, view, requests); ImGui::EndPopup(); }
                    ImGui::PopID();
                }
                DeleteCollectionPopup(state, document, writable, view, requests);
            }
            ImGui::EndChild();
            ImGui::SameLine();
            if (ImGui::BeginChild("CollectionDetails", {0, 0}, ImGuiChildFlags_Borders)) {
                const auto found = std::ranges::find(document.collections, state.selectedCollection, &RecordCollection::name);
                if (found == document.collections.end()) DisabledWrapped(localize("Workspace", "sSelectCollection", "Choose a collection on the left."));
                else DrawCollectionRecords(state, document, static_cast<std::size_t>(found - document.collections.begin()), writable, view, requests);
            }
            ImGui::EndChild();
            ImportPopup(state, document, writable, view, requests);
        }
        ImGui::End();
    }

    void DrawAddToCollectionPopup(WorkspaceViewState& state, const WorkspaceDocument& document,
        const BrowserView& view, bool writable, WorkspaceViewRequests& requests)
    {
        const auto& localize = view.localize;
        const auto title = std::string(localize("General", "sAddToCollection", "Add to Collection")) + "###AddToCollection";
        if (state.collectRequested) {
            if (!ModalUtils::CanOpenPopup("###AddToCollection")) return;
            ImGui::OpenPopup(title.c_str());
            state.collectRequested = false;
            state.name[0] = '\0';
        }
        const auto* viewport = ImGui::GetMainViewport();
        const ImVec2 maximum{viewport->WorkSize.x - 24, viewport->WorkSize.y - 24};
        const ModalUtils::PopupSizing sizing({(std::min)(440.0f, maximum.x), (std::min)(400.0f, maximum.y)},
            {(std::min)(320.0f, maximum.x), (std::min)(240.0f, maximum.y)}, maximum, false);
        if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings)) return;
        const auto close = [&] { state.collect.clear(); ImGui::CloseCurrentPopup(); };
        if (!GamepadInput::IsSteamKeyboardOpen() && ModalUtils::CancelPopupRequested()) close();
        ImGui::TextDisabled("%s: %zu", localize("General", "sSelected", "Selected"), state.collect.size());
        const float footer = ImGui::GetFrameHeightWithSpacing() * 2.0f + ImGui::GetStyle().ItemSpacing.y;
        if (ImGui::BeginChild("CollectionChoices", {0, (std::max)(ImGui::GetFrameHeight(), ImGui::GetContentRegionAvail().y - footer)}, ImGuiChildFlags_Borders)) {
            if (document.collections.empty()) DisabledWrapped(localize("Workspace", "sNoCollections", "No collections yet."));
            ImGui::BeginDisabled(!writable || state.collect.empty());
            for (std::size_t i = 0; i < document.collections.size(); ++i) {
                const auto& collection = document.collections[i];
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable((collection.name + " (" + std::to_string(collection.records.size()) + ")").c_str())) {
                    AddRecords(Edit(requests, document).collections[i], state.collect, view);
                    close();
                }
                ImGui::PopID();
            }
            ImGui::EndDisabled();
        }
        ImGui::EndChild();
        const auto* createLabel = localize("Workspace", "sCreateAndAdd", "Create and Add");
        const float createWidth = ImGui::CalcTextSize(createLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::BeginDisabled(!writable);
        const bool enter = NameInput("##NewCollection", state.name, localize("Workspace", "sNewCollection", "New Collection"),
            (std::max)(ImGui::GetFontSize() * 6.0f, ImGui::GetContentRegionAvail().x - createWidth - ImGui::GetStyle().ItemSpacing.x));
        const bool valid = !state.collect.empty() && ValidName(document.collections, state.name.data());
        ImGui::SameLine();
        ImGui::BeginDisabled(!valid);
        if (ImGui::Button(createLabel) || (enter && valid)) {
            auto& collections = Edit(requests, document).collections;
            collections.push_back({state.name.data(), {}});
            AddRecords(collections.back(), state.collect, view);
            close();
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        if (ImGui::Button(localize("General", "sCancel", "Cancel"))) close();
        ImGui::EndPopup();
    }
}
