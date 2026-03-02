#include "Data/DataManager.h"

#include "Config/Config.h"
#include "Logging/Logger.h"

#include <concepts>
#include <cctype>

#include <RE/T/TESAmmo.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESFullName.h>
#include <RE/T/TESFurniture.h>
#include <RE/T/TESObjectACTI.h>
#include <RE/T/TESObjectARMO.h>
#include <RE/T/TESObjectCONT.h>
#include <RE/T/TESObjectREFR.h>
#include <RE/T/TESObjectMISC.h>
#include <RE/T/TESObjectSTAT.h>
#include <RE/T/TESObjectWEAP.h>
#include <RE/T/TESNPC.h>

#include <RE/B/BGSPerk.h>
#include <RE/B/BGSKeywordForm.h>
#include <RE/S/SpellItem.h>

namespace ESPExplorerAE
{
    namespace
    {
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

                std::string lower(editorId);
                std::ranges::transform(lower, lower.begin(), [](unsigned char ch) {
                    return static_cast<char>(std::tolower(ch));
                });

                if (lower.contains("nonplayable") || lower.contains("non_playable") || lower.contains("non-playable")) {
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
    }

    void DataManager::Refresh()
    {
        Logger::Verbose("Data refresh started");

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            Logger::Warn("Data refresh aborted: TESDataHandler unavailable");
            return;
        }

        std::vector<PluginInfo> newPlugins;
        newPlugins.reserve(dataHandler->compiledFileCollection.files.size() + dataHandler->compiledFileCollection.smallFiles.size());

        FormCache newCache{};
        std::unordered_map<std::uint32_t, std::uint32_t> newPlacedReferenceCounts{};

        {
            const auto& [allFormsMap, allFormsLock] = RE::TESForm::GetAllForms();
            RE::BSAutoReadLock lock{ allFormsLock };

            if (allFormsMap) {
                newCache.allRecords.reserve(allFormsMap->size());

                for (const auto& [formID, form] : *allFormsMap) {
                    if (!form || formID == 0) {
                        continue;
                    }

                    if (const auto* refr = form->As<RE::TESObjectREFR>()) {
                        if (const auto* baseObject = refr->GetObjectReference()) {
                            ++newPlacedReferenceCounts[baseObject->GetFormID()];
                        }
                    }

                    FormEntry entry{};
                    entry.formID = formID;
                    entry.name = GetFormName(form);
                    entry.sourcePlugin = GetSourcePluginName(form);
                    entry.isDeleted = form->IsDeleted();
                    entry.isPlayable = IsPlayable(form);

                    const auto* formTypeString = form->GetFormTypeString();
                    entry.category = formTypeString ? formTypeString : "Other";

                    newCache.allRecords.push_back(std::move(entry));
                }
            }
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
            lightPlugins.push_back(std::move(info));
        }

        std::sort(lightPlugins.begin(), lightPlugins.end(), [](const PluginInfo& left, const PluginInfo& right) {
            return left.lightOrder < right.lightOrder;
        });

        newPlugins.insert(newPlugins.end(), lightPlugins.begin(), lightPlugins.end());

        for (auto* form : dataHandler->GetFormArray<RE::TESObjectWEAP>()) {
            if (!form) {
                continue;
            }

            FormEntry entry{};
            entry.formID = form->GetFormID();
            entry.category = "Weapon";
            entry.name = GetFormName(form);
            entry.sourcePlugin = GetSourcePluginName(form);
            entry.isDeleted = form->IsDeleted();
            entry.isPlayable = IsPlayable(form);

            if (PassesFilters(form, entry.name, entry.isPlayable)) {
                newCache.weapons.push_back(entry);
            }
        }

        for (auto* form : dataHandler->GetFormArray<RE::TESObjectARMO>()) {
            if (!form) {
                continue;
            }

            FormEntry entry{};
            entry.formID = form->GetFormID();
            entry.category = "Armor";
            entry.name = GetFormName(form);
            entry.sourcePlugin = GetSourcePluginName(form);
            entry.isDeleted = form->IsDeleted();
            entry.isPlayable = IsPlayable(form);

            if (PassesFilters(form, entry.name, entry.isPlayable)) {
                newCache.armors.push_back(entry);
            }
        }

        for (auto* form : dataHandler->GetFormArray<RE::TESAmmo>()) {
            if (!form) {
                continue;
            }

            FormEntry entry{};
            entry.formID = form->GetFormID();
            entry.category = "Ammo";
            entry.name = GetFormName(form);
            entry.sourcePlugin = GetSourcePluginName(form);
            entry.isDeleted = form->IsDeleted();
            entry.isPlayable = IsPlayable(form);

            if (PassesFilters(form, entry.name, entry.isPlayable)) {
                newCache.ammo.push_back(entry);
            }
        }

        for (auto* form : dataHandler->GetFormArray<RE::TESObjectMISC>()) {
            if (!form) {
                continue;
            }

            FormEntry entry{};
            entry.formID = form->GetFormID();
            entry.category = "Misc";
            entry.name = GetFormName(form);
            entry.sourcePlugin = GetSourcePluginName(form);
            entry.isDeleted = form->IsDeleted();
            entry.isPlayable = IsPlayable(form);

            if (PassesFilters(form, entry.name, entry.isPlayable)) {
                newCache.misc.push_back(entry);
            }
        }

        for (auto* form : dataHandler->GetFormArray<RE::TESNPC>()) {
            if (!form) {
                continue;
            }

            FormEntry entry{};
            entry.formID = form->GetFormID();
            entry.category = "NPC";
            entry.name = GetFormName(form);
            entry.sourcePlugin = GetSourcePluginName(form);
            entry.isDeleted = form->IsDeleted();
            entry.isPlayable = IsPlayable(form);

            if (PassesFilters(form, entry.name, entry.isPlayable)) {
                newCache.npcs.push_back(entry);
            }
        }

        for (auto* form : dataHandler->GetFormArray<RE::TESObjectACTI>()) {
            if (!form) {
                continue;
            }

            FormEntry entry{};
            entry.formID = form->GetFormID();
            entry.category = "Activator";
            entry.name = GetFormName(form);
            entry.sourcePlugin = GetSourcePluginName(form);
            entry.isDeleted = form->IsDeleted();
            entry.isPlayable = IsPlayable(form);

            if (PassesFilters(form, entry.name, entry.isPlayable)) {
                newCache.activators.push_back(entry);
            }
        }

        for (auto* form : dataHandler->GetFormArray<RE::TESObjectCONT>()) {
            if (!form) {
                continue;
            }

            FormEntry entry{};
            entry.formID = form->GetFormID();
            entry.category = "Container";
            entry.name = GetFormName(form);
            entry.sourcePlugin = GetSourcePluginName(form);
            entry.isDeleted = form->IsDeleted();
            entry.isPlayable = IsPlayable(form);

            if (PassesFilters(form, entry.name, entry.isPlayable)) {
                newCache.containers.push_back(entry);
            }
        }

        for (auto* form : dataHandler->GetFormArray<RE::TESObjectSTAT>()) {
            if (!form) {
                continue;
            }

            FormEntry entry{};
            entry.formID = form->GetFormID();
            entry.category = "Static";
            entry.name = GetFormName(form);
            entry.sourcePlugin = GetSourcePluginName(form);
            entry.isDeleted = form->IsDeleted();
            entry.isPlayable = IsPlayable(form);

            if (PassesFilters(form, entry.name, entry.isPlayable)) {
                newCache.statics.push_back(entry);
            }
        }

        for (auto* form : dataHandler->GetFormArray<RE::TESFurniture>()) {
            if (!form) {
                continue;
            }

            FormEntry entry{};
            entry.formID = form->GetFormID();
            entry.category = "Furniture";
            entry.name = GetFormName(form);
            entry.sourcePlugin = GetSourcePluginName(form);
            entry.isDeleted = form->IsDeleted();
            entry.isPlayable = IsPlayable(form);

            if (PassesFilters(form, entry.name, entry.isPlayable)) {
                newCache.furniture.push_back(entry);
            }
        }

        for (auto* form : dataHandler->GetFormArray<RE::SpellItem>()) {
            if (!form) {
                continue;
            }

            FormEntry entry{};
            entry.formID = form->GetFormID();
            entry.category = "Spell";
            entry.name = GetFormName(form);
            entry.sourcePlugin = GetSourcePluginName(form);
            entry.isDeleted = form->IsDeleted();
            entry.isPlayable = IsPlayable(form);

            if (PassesFilters(form, entry.name, entry.isPlayable)) {
                newCache.spells.push_back(entry);
            }
        }

        for (auto* form : dataHandler->GetFormArray<RE::BGSPerk>()) {
            if (!form) {
                continue;
            }

            FormEntry entry{};
            entry.formID = form->GetFormID();
            entry.category = "Perk";
            entry.name = GetFormName(form);
            entry.sourcePlugin = GetSourcePluginName(form);
            entry.isDeleted = form->IsDeleted();
            entry.isPlayable = IsPlayable(form);

            if (PassesFilters(form, entry.name, entry.isPlayable)) {
                newCache.perks.push_back(entry);
            }
        }

        FormCategoryCounts newCounts{};
        newCounts.weapons = newCache.weapons.size();
        newCounts.armors = newCache.armors.size();
        newCounts.ammo = newCache.ammo.size();
        newCounts.misc = newCache.misc.size();
        newCounts.npcs = newCache.npcs.size();
        newCounts.activators = newCache.activators.size();
        newCounts.containers = newCache.containers.size();
        newCounts.statics = newCache.statics.size();
        newCounts.furniture = newCache.furniture.size();
        newCounts.spells = newCache.spells.size();
        newCounts.perks = newCache.perks.size();

        {
            std::unique_lock lock(dataMutex);
            plugins = std::move(newPlugins);
            formCache = std::move(newCache);
            placedReferenceCounts = std::move(newPlacedReferenceCounts);
            counts = newCounts;
            ++dataVersion;
        }

        Logger::Verbose(
            "Data refresh finished: forms=" + std::to_string(counts.weapons + counts.armors + counts.ammo + counts.misc + counts.npcs + counts.activators + counts.containers + counts.statics + counts.furniture + counts.spells + counts.perks));
    }

    DataManager::DataView DataManager::GetDataView()
    {
        DataView view{
            .lock = std::shared_lock<std::shared_mutex>(dataMutex),
            .plugins = &plugins,
            .counts = &counts,
            .formCache = &formCache,
            .dataVersion = &dataVersion
        };
        return view;
    }

    std::uint64_t DataManager::GetDataVersion()
    {
        std::shared_lock lock(dataMutex);
        return dataVersion;
    }

    std::vector<PluginInfo> DataManager::GetPlugins()
    {
        std::shared_lock lock(dataMutex);
        return plugins;
    }

    FormCategoryCounts DataManager::GetCounts()
    {
        std::shared_lock lock(dataMutex);
        return counts;
    }

    FormCache DataManager::GetFormCache()
    {
        std::shared_lock lock(dataMutex);
        return formCache;
    }

    std::uint32_t DataManager::GetPlacedReferenceCount(std::uint32_t formID)
    {
        std::shared_lock lock(dataMutex);
        const auto it = placedReferenceCounts.find(formID);
        if (it == placedReferenceCounts.end()) {
            return 0;
        }

        return it->second;
    }

    bool DataManager::PassesFilters(RE::TESForm* form, std::string_view name, bool isPlayable)
    {
        if (!form) {
            return false;
        }

        const auto& settings = Config::Get();

        if (settings.hideDeleted && form->IsDeleted()) {
            return false;
        }

        if (settings.hideNoName && name.empty()) {
            return false;
        }

        if (settings.hideNonPlayable && !isPlayable) {
            return false;
        }

        return true;
    }

    std::string DataManager::GetFormName(RE::TESForm* form)
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

    std::string DataManager::GetSourcePluginName(RE::TESForm* form)
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

    bool DataManager::IsPlayable(RE::TESForm* form)
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
