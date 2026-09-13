#include "Core/Profiling.h"
#include "Game/CatalogReader.h"
#include "Game/ArchiveReader.h"
#include "Core/RuntimeDiagnostics.h"
#include "pch.h"



#include <concepts>
#include <cctype>
#include <cstdio>
#include <unordered_map>

#include <RE/T/TESAmmo.h>
#include <RE/B/BGSComponent.h>
#include <RE/B/BGSConstructibleObject.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESFullName.h>
#include <RE/T/TESFurniture.h>
#include <RE/T/TESFaction.h>
#include <RE/T/TESObjectACTI.h>
#include <RE/T/TESObjectARMO.h>
#include <RE/T/TESObjectCONT.h>
#include <RE/T/TESObjectREFR.h>
#include <RE/T/TESObjectMISC.h>
#include <RE/T/TESObjectSTAT.h>
#include <RE/T/TESObjectWEAP.h>
#include <RE/T/TESNPC.h>
#include <RE/T/TESRace.h>
#include <RE/T/TESWeightForm.h>
#include <RE/T/TESValueForm.h>
#include <RE/T/TESWorldSpace.h>

#include <RE/B/BGSPerk.h>
#include <RE/B/BGSKeywordForm.h>
#include <RE/B/BGSKeyword.h>
#include <RE/S/SpellItem.h>
#include <RE/T/TESObjectCELL.h>

namespace ESPExplorerAE
{
    namespace
    {
        char FoldCase(unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        }

        bool ContainsCaseInsensitive(std::string_view text, std::string_view query)
        {
            if (query.empty()) {
                return true;
            }

            const auto match = std::search(text.begin(), text.end(), query.begin(), query.end(), [](char left, char right) {
                return FoldCase(static_cast<unsigned char>(left)) == FoldCase(static_cast<unsigned char>(right));
            });

            return match != text.end();
        }

        std::string FormatPluginFormIDPrefix(const RE::TESFile& file)
        {
            char buffer[16]{};
            if (file.IsLight()) {
                std::snprintf(buffer, sizeof(buffer), "FE %03X", file.GetSmallFileCompileIndex());
            } else {
                std::snprintf(buffer, sizeof(buffer), "%02X", file.GetCompileIndex());
            }

            return buffer;
        }

        void PopulateMasterDiagnostics(RE::TESDataHandler* dataHandler, RE::TESFile* file, PluginInfo& info)
        {
            if (!dataHandler || !file || file->masterCount == 0) {
                return;
            }

            info.masters.reserve(file->masterCount);
            info.missingMasters.reserve(file->masterCount);

            for (auto* masterName : file->masters) {
                if (!masterName || masterName[0] == '\0') {
                    continue;
                }

                info.masters.emplace_back(masterName);

                if (!dataHandler->LookupLoadedModByName(masterName) && !dataHandler->LookupLoadedLightModByName(masterName)) {
                    info.missingMasters.emplace_back(masterName);
                }
            }
        }

        bool HasNonPlayableKeyword(const RE::TESForm* form)
        {
            if (!form) {
                return false;
            }

            const auto keywordForm = form->As<RE::BGSKeywordForm>();
            if (!keywordForm) {
                return false;
            }

            if (keywordForm->HasKeywordString("NonPlayable") ||
                keywordForm->HasKeywordString("Non-Playable") ||
                keywordForm->HasKeywordString("NonPlayableObject")) {
                return true;
            }

            bool found = false;
            keywordForm->ForEachKeyword([&found](RE::BGSKeyword* keyword) {
                if (!keyword) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                const auto editorId = keyword->formEditorID.c_str();
                if (!editorId) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                if (ContainsCaseInsensitive(editorId, "nonplayable") ||
                    ContainsCaseInsensitive(editorId, "non_playable") ||
                    ContainsCaseInsensitive(editorId, "non-playable")) {
                    found = true;
                    return RE::BSContainer::ForEachResult::kStop;
                }

                return RE::BSContainer::ForEachResult::kContinue;
            });

            return found;
        }

        template <class T>
        bool GetPlayableIfAvailable(const T* form)
        {
            if constexpr (requires(const T* item) {
                              { item->GetPlayable() } -> std::convertible_to<bool>;
                          }) {
                return form->GetPlayable();
            } else {
                return true;
            }
        }

        std::string GetNPCRaceName(const RE::TESNPC* npc)
        {
            if (!npc) {
                return {};
            }

            const auto* race = npc->GetFormRace();
            if (!race) {
                return {};
            }

            const auto* raceName = race->GetFullName();
            if (raceName && raceName[0] != '\0') {
                return raceName;
            }

            const auto* raceEditorID = race->GetFormEditorID();
            if (raceEditorID && raceEditorID[0] != '\0') {
                return raceEditorID;
            }

            return {};
        }

        std::string GetFactionDisplayName(const RE::TESFaction* faction)
        {
            if (!faction) {
                return {};
            }

            const auto* factionName = faction->GetFullName();
            if (factionName && factionName[0] != '\0') {
                return factionName;
            }

            const auto* factionEditorID = faction->GetFormEditorID();
            if (factionEditorID && factionEditorID[0] != '\0') {
                return factionEditorID;
            }

            return {};
        }

        std::string GetNPCFactionList(const RE::TESNPC* npc, std::vector<std::string>& names)
        {
            if (!npc) {
                return {};
            }

            std::string result;
            std::unordered_set<std::string> addedFactionLabels;

            for (const auto& factionRank : npc->factions) {
                if (!factionRank.faction || factionRank.rank < 0) {
                    continue;
                }

                const std::string factionLabel = GetFactionDisplayName(factionRank.faction);
                if (factionLabel.empty() || !addedFactionLabels.insert(factionLabel).second) {
                    continue;
                }

                if (!result.empty()) {
                    result += ", ";
                }
                names.push_back(factionLabel);
                result += factionLabel;
            }

            return result;
        }

    }

    std::shared_ptr<CatalogSnapshot> CatalogReader::Capture()
    {
        const ProfileScope profileScope(ProfileMetric::CatalogCapture);
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return {};
        auto snapshot = std::make_shared<CatalogSnapshot>();
        std::vector<PluginInfo> newPlugins;
        newPlugins.reserve(dataHandler->compiledFileCollection.files.size() + dataHandler->compiledFileCollection.smallFiles.size());
        std::unordered_set<std::uint32_t> seen;
        const auto capture = [&](RE::TESForm* form) {
            if (!form || !form->GetFormID() || !seen.insert(form->GetFormID()).second) return;
            FormEntry entry;
            entry.formID = form->GetFormID();
            entry.name = GetFormName(form);
            entry.sourcePlugin = GetSourcePluginName(form);
            entry.isDeleted = form->IsDeleted();
            entry.isPlayable = IsPlayable(form);
            if (const auto* component = form->As<RE::BGSComponent>(); component && component->scrapItem) entry.componentItemID = component->scrapItem->GetFormID();
            // The signature table is indexed by the engine enum, so guard
            // runtime-only values outside the ordinary form-type range.
            const auto type = std::to_underlying(form->GetFormType());
            if (type >= 0 && type < std::to_underlying(RE::ENUM_FORM_ID::kTotal)) {
                if (const auto* signature = form->GetFormTypeString()) entry.category = signature;
            }
            if (const auto* editorID = form->GetFormEditorID()) entry.editorID = editorID;
            if (const auto* weight = form->As<RE::TESWeightForm>()) entry.weight = weight->GetFormWeight();
            if (const auto* value = form->As<RE::TESValueForm>()) entry.value = value->GetFormValue();
            if (const auto* weapon = form->As<RE::TESObjectWEAP>()) {
                entry.baseDamage = weapon->weaponData.attackDamage;
                entry.weight = weapon->weaponData.weight;
                entry.value = weapon->weaponData.value;
                if (const auto* ammo = weapon->weaponData.ammo) {
                    entry.weaponAmmoID = ammo->GetFormID();
                    entry.weaponAmmoName = RE::TESFullName::GetFullName(*ammo);
                    if (entry.weaponAmmoName.empty() && ammo->GetFormEditorID()) entry.weaponAmmoName = ammo->GetFormEditorID();
                }
            }
            if (const auto* armor = form->As<RE::TESObjectARMO>()) { entry.weight = armor->armorData.weight; entry.value = armor->armorData.value; entry.armorRating = armor->armorData.rating; }
            if (const auto* recipe = form->As<RE::BGSConstructibleObject>()) {
                if (recipe->createdItem) entry.relationships.push_back({RelationshipKind::RecipeOutput, recipe->createdItem->GetFormID(), recipe->data.numConstructed});
                if (recipe->requiredItems) for (const auto& pair : *recipe->requiredItems) {
                    if (entry.relationships.size() >= 1024) { entry.relationshipsTruncated = true; break; }
                    if (pair.first) entry.relationships.push_back({RelationshipKind::RecipeInput, pair.first->GetFormID(), pair.second.i});
                }
            }
            if (const auto* cell = form->As<RE::TESObjectCELL>()) {
                entry.cellInterior = cell->IsInterior();
                if (cell->IsExterior() && cell->worldSpace) {
                    entry.worldspace = RE::TESFullName::GetFullName(*cell->worldSpace);
                    if (entry.worldspace.empty() && cell->worldSpace->GetFormEditorID()) entry.worldspace = cell->worldSpace->GetFormEditorID();
                }
            }
            if (const auto* keywords = form->As<RE::BGSKeywordForm>()) {
                keywords->ForEachKeyword([&](RE::BGSKeyword* keyword) {
                    if (keyword) entry.keywordIDs.push_back(keyword->GetFormID());
                    if (keyword && !keyword->formEditorID.empty()) entry.keywords.emplace_back(keyword->formEditorID.c_str());
                    return RE::BSContainer::ForEachResult::kContinue;
                });
                std::ranges::sort(entry.keywords);
                entry.keywords.erase(std::unique(entry.keywords.begin(), entry.keywords.end()), entry.keywords.end());
            }
            if (const auto* npc = form->As<RE::TESNPC>()) {
                entry.race = GetNPCRaceName(npc);
                entry.factions = GetNPCFactionList(npc, entry.factionNames);
                entry.hasNPCData = true;
                entry.npcEssential = npc->IsEssential();
                entry.npcUnique = npc->IsUnique();
                entry.npcProtected = npc->IsProtected();
                entry.npcFemale = npc->IsFemale();
            }
            if (const auto* refr = form->As<RE::TESObjectREFR>()) {
                if (const auto* base = refr->GetObjectReference()) ++snapshot->runtimeReferenceCounts[base->GetFormID()];
            }
            if (form->As<RE::BGSKeyword>() && !entry.editorID.empty()) snapshot->availableKeywords.push_back(entry.editorID);
            snapshot->records.push_back(std::move(entry));
        };
        {
            const auto& [allForms, allFormsLock] = RE::TESForm::GetAllForms();
            RE::BSAutoReadLock lock{ allFormsLock };
            if (!allForms) return {};
            snapshot->records.reserve(allForms->size());
            for (const auto& [id, form] : *allForms) capture(form);
        }
        // TESDataHandler::GetFormArray<T>() indexes these same TESForm* arrays.
        // Merge every populated type, including types without a dedicated
        // browser or concrete CommonLib wrapper, into the canonical store.
        for (const auto& forms : dataHandler->formArrays) {
            for (auto* form : forms) capture(form);
        }

        for (auto* file : dataHandler->compiledFileCollection.files) {
            if (!file) {
                continue;
            }

            PluginInfo info{};
            info.filename = std::string(file->GetFilename());
            info.loadOrder = file->GetCompileIndex();
            info.lightOrder = 0;
            info.isLight = file->IsLight();
            info.type = info.isLight ? "ESL" : (file->flags.all(RE::TESFile::RecordFlag::kMaster) ? "ESM" : "ESP");
            info.formIDPrefix = FormatPluginFormIDPrefix(*file);
            PopulateMasterDiagnostics(dataHandler, file, info);
            newPlugins.push_back(std::move(info));
        }

        std::sort(newPlugins.begin(), newPlugins.end(), [](const PluginInfo& left, const PluginInfo& right) {
            return left.loadOrder < right.loadOrder;
        });

        std::vector<PluginInfo> lightPlugins;
        lightPlugins.reserve(dataHandler->compiledFileCollection.smallFiles.size());

        for (auto* file : dataHandler->compiledFileCollection.smallFiles) {
            if (!file) {
                continue;
            }

            PluginInfo info{};
            info.filename = std::string(file->GetFilename());
            info.loadOrder = 0xFE;
            info.lightOrder = file->GetSmallFileCompileIndex();
            info.isLight = true;
            info.type = "ESL";
            info.formIDPrefix = FormatPluginFormIDPrefix(*file);
            PopulateMasterDiagnostics(dataHandler, file, info);
            lightPlugins.push_back(std::move(info));
        }

        std::sort(lightPlugins.begin(), lightPlugins.end(), [](const PluginInfo& left, const PluginInfo& right) {
            return left.lightOrder < right.lightOrder;
        });

        newPlugins.insert(newPlugins.end(), lightPlugins.begin(), lightPlugins.end());

        snapshot->plugins = std::move(newPlugins);
        snapshot->loadedArchives = ArchiveReader::Capture();
        PopulateRuntimeDiagnostics(*snapshot);
        snapshot->BuildIndexes();
        snapshot->ready = true;
        return snapshot;
    }

    std::string CatalogReader::GetFormName(RE::TESForm* form)
    {
        if (!form) {
            return {};
        }

        const auto name = RE::TESFullName::GetFullName(*form);
        if (name.empty()) {
            return {};
        }

        return std::string(name);
    }

    std::string CatalogReader::GetSourcePluginName(RE::TESForm* form)
    {
        if (!form) {
            return {};
        }

        const auto* file = form->GetFile(0);
        if (!file) {
            return {};
        }

        const auto filename = file->GetFilename();
        if (filename.empty()) {
            return {};
        }

        return std::string(filename);
    }

    bool CatalogReader::IsPlayable(RE::TESForm* form)
    {
        if (!form) {
            return false;
        }

        const bool formPlayable = form->GetPlayable(nullptr);
        if (!formPlayable) {
            return false;
        }

        if (HasNonPlayableKeyword(form)) {
            return false;
        }

        switch (form->GetFormType()) {
        case RE::ENUM_FORM_ID::kWEAP:
            return GetPlayableIfAvailable(static_cast<const RE::TESObjectWEAP*>(form));
        case RE::ENUM_FORM_ID::kARMO:
            return GetPlayableIfAvailable(static_cast<const RE::TESObjectARMO*>(form));
        case RE::ENUM_FORM_ID::kAMMO:
            return GetPlayableIfAvailable(static_cast<const RE::TESAmmo*>(form));
        default:
            return true;
        }
    }
}
