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
            const bool open = ImGui::TreeNodeEx(id, ImGuiTreeNodeFlags_SpanAvailWidth, "%s", "");
            ImGui::SameLine(labelX);
            ImGui::TextWrapped("%s", label);
            return open;
        }

        template<class T> bool UniqueName(const std::vector<T>& groups, std::string_view name)
        {
            return !name.empty() && name.size() <= 128 && std::ranges::none_of(groups, [&](const auto& group) { return FoldSearchText(group.name) == FoldSearchText(name); });
        }
        template<class T> void AppendGroups(std::vector<T>& target, std::vector<T> source)
        {
            for (auto& group : source) {
                const auto base = group.name;
                for (int suffix = 2; !UniqueName(target, group.name); ++suffix) group.name = base.substr(0, 110) + " (" + std::to_string(suffix) + ")";
                target.push_back(std::move(group));
            }
        }
    }
    void DrawWorkspaceSources(WorkspaceViewState& state, const WorkspaceDocument& document, RecordScope& scope,
        const BrowserView& view, WorkspaceViewRequests& requests)
    {
        const auto& localize = view.localize;
        ImGui::TextWrapped("%s", localize("Workspace", "sManage", "Views & Collections"));
        if (ImGui::Button(localize("General", "sOpen", "Open"), {-1, 0})) { state.open = true; state.focusPending = true; }
        if (SourceGroup("SourceViews", localize("Workspace", "sSavedViews", "Saved Views"))) {
            for (const auto& saved : document.views) if (ImGui::Selectable(saved.name.c_str()))
                requests.restore = ResolveSavedView(saved, *view.catalog, view.session);
            ImGui::TreePop();
        }
        if (SourceGroup("SourceCollections", localize("Workspace", "sCollections", "Collections"))) {
            for (const auto& collection : document.collections) if (ImGui::Selectable(collection.name.c_str(), scope.kind == SearchScope::Collection && scope.collection == collection.name)) {
                scope.kind = SearchScope::Collection; scope.collection = collection.name;
                ResolveCollectionScope(scope, document, *view.catalog, view.session);
            }
            ImGui::TreePop();
        }
        if (SourceGroup("SourceKits", localize("Workspace", "sItemKits", "Item Kits"))) {
            for (const auto& kit : document.kits) if (ImGui::Selectable(kit.name.c_str())) requests.loadKit = kit;
            ImGui::TreePop();
        }
    }
    void DrawWorkspaceWindow(WorkspaceViewState& state, WorkspaceDocument& document, const WorkspaceLocation& current,
        const BrowserView& view, bool writable, WorkspaceViewRequests& requests)
    {
        if (!state.open) return;
        const auto& localize = view.localize;
        const std::string title = std::string(localize("Workspace", "sManage", "Views & Collections")) + "###WorkspaceManager";
        ModalUtils::PrepareToolWindow(title.c_str(), {720, 660}, {420, 300}, state.focusPending);
        if (ImGui::Begin(title.c_str(), &state.open, ImGuiWindowFlags_NoCollapse)) {
            if (ModalUtils::EscapeClosesCurrentWindow()) state.open = false;
            if (!writable) ImGui::TextWrapped("%s", localize("Workspace", "sReadOnly", "Workspace could not be loaded. Saving is disabled; you can still export."));
            if (state.failed || state.storageFailed) ImGui::TextWrapped("%s", localize("Workspace", "sSaveFailed", "Changes could not be saved. Check names, input limits, and workspace file access."));
            ImGui::InputText(localize("General", "sName", "Name"), state.name.data(), state.name.size());
            SearchBar::ReadControllerText(localize("General", "sName", "Name"), state.name.data(), state.name.size());
            ImGui::BeginDisabled(!writable);
            const bool validView = DestinationFor(current.page) == WorkspaceDestination::Explore && (!current.query.structuredSearch || ParseSearch(current.query.search));
            ImGui::BeginDisabled(!validView || !UniqueName(document.views, state.name.data()));
            if (ImGui::Button(localize("Workspace", "sSaveView", "Save Current View"))) {
                document.views.push_back(CaptureSavedView(state.name.data(), current, *view.catalog, view.session));
                requests.changed = true;
            }
            ImGui::EndDisabled();
            ImGuiWidgetUtils::DrawWrappedSameLine(localize("Workspace", "sNewCollection", "New Collection"));
            ImGui::BeginDisabled(!UniqueName(document.collections, state.name.data()));
            if (ImGui::Button(localize("Workspace", "sNewCollection", "New Collection"))) {
                document.collections.push_back({state.name.data(), {}});
                state.selectedCollection = state.name.data(); requests.changed = true;
            }
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            if (ImGui::CollapsingHeader(localize("Workspace", "sSavedViews", "Saved Views"), ImGuiTreeNodeFlags_DefaultOpen)) {
                for (std::size_t i = 0; i < document.views.size(); ++i) {
                    ImGui::PushID(static_cast<int>(i));
                    auto& saved = document.views[i];
                    if (ImGui::Selectable(saved.name.c_str())) requests.restore = ResolveSavedView(saved, *view.catalog, view.session);
                    if (BeginEntryContext()) {
                        ImGui::BeginDisabled(!writable);
                        if (ImGui::MenuItem(localize("Workspace", "sRename", "Rename to Name Above"), nullptr, false, UniqueName(document.views, state.name.data()))) { saved.name = state.name.data(); requests.changed = true; }
                        if (ImGui::MenuItem(localize("General", "sRemove", "Remove"))) { document.views.erase(document.views.begin() + i); requests.changed = true; }
                        ImGui::EndDisabled(); ImGui::EndPopup();
                    }
                    ImGui::PopID();
                }
            }
            if (ImGui::CollapsingHeader(localize("Workspace", "sItemKits", "Item Kits"))) {
                std::optional<std::size_t> remove;
                for (std::size_t i = 0; i < document.kits.size(); ++i) {
                    ImGui::PushID("SavedKit"); ImGui::PushID(static_cast<int>(i));
                    auto& kit = document.kits[i];
                    if (ImGui::Selectable(kit.name.c_str())) requests.loadKit = kit;
                    if (BeginEntryContext()) {
                        ImGui::BeginDisabled(!writable);
                        if (ImGui::MenuItem(localize("Workspace", "sRename", "Rename to Name Above"), nullptr, false, UniqueName(document.kits, state.name.data()))) {
                            kit.name = state.name.data(); requests.changed = true;
                        }
                        if (ImGui::MenuItem(localize("General", "sRemove", "Remove"))) remove = i;
                        ImGui::EndDisabled(); ImGui::EndPopup();
                    }
                    ImGui::PopID(); ImGui::PopID();
                }
                if (remove) { document.kits.erase(document.kits.begin() + *remove); requests.changed = true; }
            }
            if (ImGui::BeginCombo(localize("Workspace", "sCollection", "Collection"), state.selectedCollection.c_str())) {
                for (const auto& collection : document.collections) if (ImGui::Selectable(collection.name.c_str(), collection.name == state.selectedCollection)) {
                    state.selectedCollection = collection.name; state.noteIndex = static_cast<std::size_t>(-1);
                }
                ImGui::EndCombo();
            }
            const auto found = std::ranges::find(document.collections, state.selectedCollection, &RecordCollection::name);
            if (found != document.collections.end()) {
                auto& collection = *found;
                ImGui::BeginDisabled(!writable);
                const auto& targets = state.collect.empty() ? current.selection : state.collect;
                ImGui::BeginDisabled(targets.empty());
                if (ImGui::Button(localize("Workspace", "sAddSelection", "Add Selection to Collection"))) {
                    for (auto id : targets) {
                        auto record = CaptureWorkspaceRecord(*view.catalog, id, view.session);
                        if (std::ranges::none_of(collection.records, [&](const auto& existing) {
                            return record.identity.empty() ? existing.identity.empty() && existing.transientID == id && existing.session == view.session : existing.identity == record.identity;
                        })) collection.records.push_back(std::move(record));
                    }
                    state.collect.clear(); requests.changed = true;
                }
                ImGui::EndDisabled();
                ImGuiWidgetUtils::DrawWrappedSameLine(localize("Workspace", "sRename", "Rename to Name Above"));
                if (ImGui::Button(localize("Workspace", "sRename", "Rename to Name Above")) && UniqueName(document.collections, state.name.data())) {
                    for (auto& saved : document.views) if (saved.location.query.scope.collection == collection.name) saved.location.query.scope.collection = state.name.data();
                    state.selectedCollection = collection.name = state.name.data(); requests.changed = true;
                }
                ImGui::EndDisabled();
                FavoriteIdentity identity(*view.catalog);
                std::optional<std::size_t> remove;
                if (ImGui::BeginChild("CollectionRecords", {0, 180}, ImGuiChildFlags_Borders)) {
                    for (std::size_t i = 0; i < collection.records.size(); ++i) {
                        auto& record = collection.records[i]; ImGui::PushID(static_cast<int>(i));
                        const auto target = ResolveWorkspaceRecord(identity, record, view.session);
                        if (ImGui::Selectable((record.name.empty() ? record.identity : record.name).c_str(), state.noteIndex == i)) {
                            state.noteIndex = i; std::snprintf(state.note.data(), state.note.size(), "%s", record.note.c_str());
                            if (target) requests.inspect.push_back(target.formID);
                        }
                        if (BeginEntryContext()) {
                            if (ImGui::MenuItem(localize("General", "sRemove", "Remove"), nullptr, false, writable)) remove = i;
                            ImGui::EndPopup();
                        }
                        if (record.identity.empty()) ImGui::TextWrapped("%s", localize("Workspace", "sSessionOnly", "Session only; not restored as a target"));
                        else if (!target) ImGui::TextWrapped("%s: %s", localize("Workspace", "sUnresolved", "Unresolved; retained for later"), record.identity.c_str());
                        ImGui::PopID();
                    }
                }
                ImGui::EndChild();
                if (remove) { collection.records.erase(collection.records.begin() + *remove); state.noteIndex = static_cast<std::size_t>(-1); requests.changed = true; }
                if (state.noteIndex < collection.records.size()) {
                    ImGui::BeginDisabled(!writable);
                    bool edited = ImGui::InputTextMultiline(localize("Workspace", "sNote", "Note"), state.note.data(), state.note.size(), {0, 65});
                    edited |= SearchBar::ReadControllerText(localize("Workspace", "sNote", "Note"), state.note.data(), state.note.size());
                    if (edited) {
                        collection.records[state.noteIndex].note = state.note.data(); requests.changed = true;
                    }
                    ImGui::EndDisabled();
                }
                if (ImGui::Button(localize("Workspace", "sDeleteCollection", "Delete Collection"))) ImGui::OpenPopup("DeleteCollection");
                if (ImGui::BeginPopup("DeleteCollection")) {
                    ImGui::TextWrapped("%s", localize("Workspace", "sDeleteCollectionConfirm", "Remove this collection and its notes? Game records are unaffected."));
                    if (ImGui::Button(localize("General", "sCancel", "Cancel"))) ImGui::CloseCurrentPopup();
                    ImGui::SameLine(); ImGui::BeginDisabled(!writable);
                    if (ImGui::Button(localize("General", "sRemove", "Remove"))) { document.collections.erase(found); state.selectedCollection.clear(); requests.changed = true; ImGui::CloseCurrentPopup(); }
                    ImGui::EndDisabled(); ImGui::EndPopup();
                }
            }
            ImGui::Separator();
            if (ImGui::Button(localize("Workspace", "sExportClipboard", "Export Workspace to Clipboard"))) {
                std::string bytes; if (WriteWorkspaceIni(document, bytes)) ImGui::SetClipboardText(bytes.c_str()); else state.failed = true;
            }
            ImGuiWidgetUtils::DrawWrappedSameLine(localize("Workspace", "sPreviewImport", "Preview Clipboard Import"));
            if (ImGui::Button(localize("Workspace", "sPreviewImport", "Preview Clipboard Import"))) {
                const char* clipboard = ImGui::GetClipboardText();
                if (clipboard) {
                    std::size_t size{}; while (size <= WorkspaceDocument::MaxBytes && clipboard[size]) ++size;
                    state.imported = size <= WorkspaceDocument::MaxBytes ? ReadWorkspaceIni({clipboard, size}) : WorkspaceReadResult{{}, "Input too large"};
                }
            }
            if (state.imported) {
                if (!*state.imported) ImGui::TextWrapped("%s", localize("Workspace", "sInvalidImport", "Import rejected: unsupported schema, invalid entries, or input too large. No changes were made."));
                else {
                    const auto& incoming = state.imported->document;
                    ImGui::Text("%s: %zu | %s: %zu | %s: %zu", localize("Workspace", "sSavedViews", "Saved Views"), incoming.views.size(),
                        localize("Workspace", "sCollections", "Collections"), incoming.collections.size(), localize("Workspace", "sItemKits", "Item Kits"), incoming.kits.size());
                    FavoriteIdentity identity(*view.catalog);
                    const auto preview = [&](const WorkspaceRecord& record) {
                        if (!ResolveWorkspaceRecord(identity, record, view.session) && (!record.identity.empty() || !record.name.empty()))
                            ImGui::BulletText("%s: %s | %s", localize("Workspace", "sUnresolved", "Unresolved; retained for later"), record.name.c_str(), record.identity.c_str());
                    };
                    if (ImGui::BeginChild("ImportMissing", {0, 95}, ImGuiChildFlags_Borders)) {
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
                    ImGui::TextWrapped("%s", localize("Workspace", "sImportHint", "Adds entries and renames duplicates. Missing records are kept; kits are saved without giving items."));
                    ImGui::BeginDisabled(!writable);
                    if (ImGui::Button(localize("Workspace", "sImport", "Import Reviewed Workspace"))) {
                        auto merged = document;
                        auto imported = incoming;
                        for (auto& collection : imported.collections) {
                            const auto previous = collection.name;
                            for (int suffix = 2; !UniqueName(merged.collections, collection.name); ++suffix) collection.name = previous.substr(0, 110) + " (" + std::to_string(suffix) + ")";
                            for (auto& saved : imported.views) if (saved.location.query.scope.collection == previous) saved.location.query.scope.collection = collection.name;
                            merged.collections.push_back(std::move(collection));
                        }
                        AppendGroups(merged.views, std::move(imported.views)); AppendGroups(merged.kits, std::move(imported.kits));
                        std::string bytes;
                        if (WriteWorkspaceIni(merged, bytes)) { document = std::move(merged); requests.changed = true; state.imported.reset(); }
                        else state.failed = true;
                    }
                    ImGui::EndDisabled();
                }
            }
        }
        ImGui::End();
    }
}
