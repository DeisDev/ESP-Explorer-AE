#include "Game/DetailReader.h"
#include "pch.h"
#include <RE/A/AlchemyItem.h>
#include <RE/B/BGSConstructibleObject.h>
#include <RE/B/BGSEncounterZone.h>
#include <RE/B/BGSKeywordForm.h>
#include <RE/B/BGSLightingTemplate.h>
#include <RE/B/BGSLocation.h>
#include <RE/B/BGSOutfit.h>
#include <RE/B/BGSBaseAlias.h>
#include <RE/B/BGSQuestObjective.h>
#include <RE/T/TESQuestTarget.h>
#include <RE/T/TESAmmo.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESFullName.h>
#include <RE/T/TESFurniture.h>
#include <RE/T/TESGlobal.h>
#include <RE/T/TESNPC.h>
#include <RE/T/TESObjectACTI.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESObjectARMO.h>
#include <RE/T/TESObjectCONT.h>
#include <RE/T/TESObjectSTAT.h>
#include <RE/T/TESObjectWEAP.h>
#include <RE/T/TESQuest.h>
#include <RE/T/TESWeather.h>
#include <RE/S/SpellItem.h>
#include <RE/T/TESSound.h>
#include <RE/T/TESValueForm.h>
#include <RE/T/TESWaterForm.h>
#include <RE/T/TESWeightForm.h>
#include <RE/T/TESWorldSpace.h>

namespace ESPExplorerAE
{
    namespace
    {
        std::string ReadString(const char* text) { return text ? std::string(text) : std::string{}; }
        DetailReference CaptureReference(const RE::TESForm* form)
        {
            if (!form) return {};
            return { form->GetFormID(), std::string(RE::TESFullName::GetFullName(*form)), ReadString(form->GetFormEditorID()) };
        }

        std::uint32_t CountContainerEntries(const RE::TESContainer* container)
        {
            return container ? container->numContainerObjects : 0;
        }

        std::uint32_t CountContainerItems(const RE::TESContainer* container)
        {
            if (!container) {
                return 0;
            }

            std::uint32_t totalItems = 0;
            container->ForEachContainerObject([&totalItems](RE::ContainerObject& entry) {
                if (entry.count > 0) {
                    totalItems += static_cast<std::uint32_t>(entry.count);
                }
                return true;
            });
            return totalItems;
        }

        WeaponDetails ReadWeapon(const RE::TESObjectWEAP* weaponForm, bool advanced)
        {
            WeaponDetails result;
            result.ammo = CaptureReference(weaponForm->weaponData.ammo);
            result.damageBase = weaponForm->weaponData.attackDamage;
            result.ammoCapacity = weaponForm->weaponData.ammoCapacity;
            result.attachmentParentCount = weaponForm->attachParents.size;
            if (weaponForm->weaponData.attackSeconds > 0.0f) result.fireRate = 1.0f / weaponForm->weaponData.attackSeconds;
            if (!advanced) return result;
            result.model = ReadString(weaponForm->model.c_str());
            result.equipmentType = CaptureReference(weaponForm->GetEquipSlot(nullptr));
            result.blockBashImpactData = CaptureReference(weaponForm->blockBashImpactDataSet);
            result.blockBashMaterial = CaptureReference(weaponForm->altBlockMaterialType);
            result.instanceNamingRules = CaptureReference(weaponForm->instanceNamingRules);
            result.impactDataSet = CaptureReference(weaponForm->weaponData.impactDataSet);
            result.onHitEffect = CaptureReference(weaponForm->weaponData.effect);
            result.skill = CaptureReference(weaponForm->weaponData.skill);
            result.resist = CaptureReference(weaponForm->weaponData.resistance);
            result.speed = weaponForm->weaponData.speed;
            result.reloadSpeed = weaponForm->weaponData.reloadSpeed;
            result.reach = weaponForm->weaponData.reach;
            result.minRange = weaponForm->weaponData.minRange;
            result.maxRange = weaponForm->weaponData.maxRange;
            result.attackDelay = weaponForm->weaponData.attackDelaySec;
            result.damageOutOfRangeMult = weaponForm->weaponData.outOfRangeDamageMult;
            result.damageOnHitMult = weaponForm->weaponData.damageToWeaponMult;
            result.damageSecondary = weaponForm->weaponData.secondaryDamage;
            result.weight = weaponForm->weaponData.weight;
            result.value = weaponForm->weaponData.value;
            result.actionPointCost = weaponForm->weaponData.attackActionPointCost;
            result.criticalChargeBonus = weaponForm->weaponData.criticalChargeBonus;
            result.criticalDamageMult = weaponForm->weaponData.criticalDamageMult;
            result.soundLevelMult = weaponForm->weaponData.soundLevelMult;
            if (weaponForm->weaponData.rangedData) result.fireSeconds = weaponForm->weaponData.rangedData->fireSeconds;
            if (weaponForm->weaponData.rangedData) result.reloadSeconds = weaponForm->weaponData.rangedData->reloadSeconds;
            if (weaponForm->weaponData.rangedData) result.projectiles = static_cast<std::uint32_t>(weaponForm->weaponData.rangedData->numProjectiles);
            if (weaponForm->weaponData.rangedData) result.overrideProjectile = CaptureReference(weaponForm->weaponData.rangedData->overrideProjectile);
            result.hasRangedData = weaponForm->weaponData.rangedData != nullptr;
            result.hasDamageTypes = weaponForm->weaponData.damageTypes != nullptr;
            if (weaponForm->weaponData.damageTypes) {
                for (const auto& pair : *weaponForm->weaponData.damageTypes) result.damageTypes.push_back({ CaptureReference(pair.first), pair.second.f });
            }
            result.hasActorValues = weaponForm->weaponData.actorValues != nullptr;
            if (weaponForm->weaponData.actorValues) {
                for (const auto& pair : *weaponForm->weaponData.actorValues) result.actorValues.push_back({ CaptureReference(pair.first), pair.second.f });
            }
            return result;
        }

        ArmorDetails ReadArmor(const RE::TESObjectARMO* armorForm, bool advanced)
        {
            ArmorDetails result;
            result.armorRating = armorForm->armorData.rating;
            result.attachmentParentCount = armorForm->attachParents.size;
            if (!advanced) return result;
            result.equipmentType = CaptureReference(armorForm->GetEquipSlot(nullptr));
            result.blockBashImpactData = CaptureReference(armorForm->blockBashImpactDataSet);
            result.blockBashMaterial = CaptureReference(armorForm->altBlockMaterialType);
            result.instanceNamingRules = CaptureReference(armorForm->instanceNamingRules);
            result.health = armorForm->armorData.health;
            result.value = armorForm->armorData.value;
            result.weight = armorForm->armorData.weight;
            result.colorRemapIndex = armorForm->armorData.colorRemappingIndex;
            result.model = ReadString(armorForm->worldModel[0].model.c_str());
            result.addonCount = armorForm->modelArray.size();
            if (armorForm->armorData.damageTypes) result.damageTypeEntries = armorForm->armorData.damageTypes->size();
            if (armorForm->armorData.actorValues) result.actorValueEntries = armorForm->armorData.actorValues->size();
            result.hasDamageTypes = armorForm->armorData.damageTypes != nullptr;
            result.hasActorValues = armorForm->armorData.actorValues != nullptr;
            return result;
        }

        NPCDetails ReadNPC(const RE::TESNPC* npcForm, bool advanced)
        {
            NPCDetails result;
            result.level = npcForm->GetLevel();
            result.resolvedRace = CaptureReference(npcForm->GetFormRace());
            result.attachmentParentCount = npcForm->attachParents.size;
            result.defaultOutfit = CaptureReference(npcForm->defOutfit);
            if (!advanced) return result;
            result.height = npcForm->height;
            result.heightMax = npcForm->heightMax;
            result.npcClass = CaptureReference(npcForm->cl);
            result.combatStyle = CaptureReference(npcForm->combatStyle);
            result.sleepOutfit = CaptureReference(npcForm->sleepOutfit);
            result.crimeFaction = CaptureReference(npcForm->crimeFaction);
            return result;
        }

        SoundDetails ReadSound(const RE::TESSound* soundForm, bool advanced)
        {
            SoundDetails result;
            result.descriptor = CaptureReference(soundForm->descriptor);
            if (!advanced) return result;
            result.minDelay = soundForm->repeatData.minDelay;
            result.maxDelay = soundForm->repeatData.maxDelay;
            result.stackable = soundForm->repeatData.stackable;
            return result;
        }

        AmmoDetails ReadAmmo(const RE::TESAmmo* ammoForm, bool advanced)
        {
            AmmoDetails result;
            result.damage = ammoForm->data.damage;
            if (!advanced) return result;
            result.model = ReadString(ammoForm->model.c_str());
            result.shellCasingModel = ReadString(ammoForm->shellCasing.model.c_str());
            result.health = ammoForm->data.health;
            result.flags = static_cast<std::uint32_t>(static_cast<unsigned char>(ammoForm->data.flags));
            result.projectile = CaptureReference(ammoForm->data.projectile);
            return result;
        }

        AlchemyDetails ReadAlchemy(const RE::AlchemyItem* alchemyForm, bool advanced)
        {
            AlchemyDetails result;
            if (!advanced) return result;
            result.model = ReadString(alchemyForm->model.c_str());
            result.equipmentType = CaptureReference(alchemyForm->GetEquipSlot(nullptr));
            result.addictionItem = CaptureReference(alchemyForm->data.addictionItem);
            result.addictionChance = alchemyForm->data.addictionChance;
            result.consumptionSound = CaptureReference(alchemyForm->data.consumptionSound);
            return result;
        }

        GlobalDetails ReadGlobal(const RE::TESGlobal* globalForm, bool advanced)
        {
            GlobalDetails result;
            if (!advanced) return result;
            result.value = globalForm->GetValue();
            return result;
        }

        OutfitDetails ReadOutfit(const RE::BGSOutfit* outfitForm, bool)
        {
            OutfitDetails result;
            result.outfitItems = static_cast<std::uint32_t>(outfitForm->outfitItems.size());
            for (const auto* item : outfitForm->outfitItems) {
                if (result.items.size() >= 1024) { result.truncated = true; break; }
                result.items.push_back({CaptureReference(item), 1});
            }
            return result;
        }

        WeatherDetails ReadWeather(const RE::TESWeather* weatherForm, bool advanced)
        {
            WeatherDetails result;
            if (!advanced) return result;
            result.flags = static_cast<std::uint32_t>(static_cast<unsigned char>(weatherForm->weatherData[static_cast<std::size_t>(RE::TESWeather::WeatherData::kFlags)]));
            result.cloudLayers = weatherForm->numCloudLayers;
            result.skyStatics = static_cast<std::uint32_t>(weatherForm->skyStatics.size());
            result.volatilityMult = weatherForm->volatilityMult;
            result.visibilityMult = weatherForm->visibilityMult;
            result.auroraModel = ReadString(weatherForm->aurora.model.c_str());
            return result;
        }

        ActivatorDetails ReadActivator(const RE::TESObjectACTI* activatorForm, bool advanced)
        {
            ActivatorDetails result;
            if (!advanced) return result;
            result.model = ReadString(activatorForm->model.c_str());
            result.materialSwap = CaptureReference(activatorForm->swapForm);
            result.colorRemapIndex = activatorForm->colorRemappingIndex;
            result.loopSound = CaptureReference(activatorForm->soundLoop);
            result.activateSound = CaptureReference(activatorForm->soundActivate);
            result.waterType = CaptureReference(activatorForm->waterForm);
            result.flags = activatorForm->flags;
            return result;
        }

        ContainerDetails ReadContainer(const RE::TESObjectCONT* containerForm, bool advanced)
        {
            ContainerDetails result;
            if (containerForm->containerObjects) containerForm->ForEachContainerObject([&](const RE::ContainerObject& entry) {
                if (result.contents.size() >= 1024) { result.truncated = true; return false; }
                result.contents.push_back({CaptureReference(entry.obj), entry.count});
                return true;
            });
            if (!advanced) return result;
            result.model = ReadString(containerForm->model.c_str());
            result.materialSwap = CaptureReference(containerForm->swapForm);
            result.colorRemapIndex = containerForm->colorRemappingIndex;
            result.weight = containerForm->weight;
            result.flags = static_cast<std::uint32_t>(static_cast<unsigned char>(containerForm->data.contFlags));
            result.containerEntries = CountContainerEntries(containerForm);
            result.containerItems = CountContainerItems(containerForm);
            result.openSound = CaptureReference(containerForm->openSound);
            result.closeSound = CaptureReference(containerForm->closeSound);
            result.takeAllSound = CaptureReference(containerForm->takeAllSound);
            result.containsOnlyList = CaptureReference(containerForm->containsOnlyList);
            return result;
        }

        StaticDetails ReadStatic(const RE::TESObjectSTAT* staticForm, bool advanced)
        {
            StaticDetails result;
            if (!advanced) return result;
            result.model = ReadString(staticForm->model.c_str());
            result.materialSwap = CaptureReference(staticForm->swapForm);
            result.colorRemapIndex = staticForm->colorRemappingIndex;
            result.materialThresholdAngle = staticForm->data.materialThresholdAngle;
            result.leafAmplitude = staticForm->data.leafAmplitude;
            result.leafFrequency = staticForm->data.leafFrequency;
            return result;
        }

        FurnitureDetails ReadFurniture(const RE::TESFurniture* furnitureForm, bool advanced)
        {
            FurnitureDetails result;
            if (!advanced) return result;
            auto* mutableFurniture = const_cast<RE::TESFurniture*>(furnitureForm);
            auto* container = mutableFurniture->GetContainer();
            result.entryPoints = static_cast<std::uint32_t>(furnitureForm->entryPointDataArray.size());
            result.markerCount = static_cast<std::uint32_t>(furnitureForm->markersArray.size());
            result.attachmentParentCount = furnitureForm->attachParents.size;
            result.flags = furnitureForm->furnFlags;
            result.workbenchType = furnitureForm->wbData.type.underlying();
            result.associatedForm = CaptureReference(furnitureForm->associatedForm);
            result.containerEntries = CountContainerEntries(container);
            result.containerItems = CountContainerItems(container);
            return result;
        }

        SpellDetails ReadSpell(const RE::SpellItem* spellForm, bool advanced)
        {
            SpellDetails result;
            if (!advanced) return result;
            result.equipmentType = CaptureReference(spellForm->GetEquipSlot(nullptr));
            result.costOverride = spellForm->data.costOverride;
            result.flags = spellForm->data.flags;
            result.castDuration = spellForm->data.castDuration;
            result.range = spellForm->data.range;
            result.chargeTime = spellForm->data.chargeTime;
            result.castingPerk = CaptureReference(spellForm->data.castingPerk);
            result.effectEntries = static_cast<std::uint32_t>(spellForm->listOfEffects.size());
            result.hostileCount = spellForm->hostileCount;
            result.preloadCount = spellForm->preloadCount;
            return result;
        }

        PerkDetails ReadPerk(const RE::BGSPerk* perkForm, bool advanced)
        {
            PerkDetails result;
            result.nextPerk = CaptureReference(perkForm->nextPerk);
            if (!advanced) return result;
            result.trait = perkForm->data.trait;
            result.playable = perkForm->data.playable;
            result.hidden = perkForm->data.hidden;
            result.level = perkForm->data.level;
            result.numRanks = perkForm->data.numRanks;
            result.perkEntries = static_cast<std::uint32_t>(perkForm->perkEntries.size());
            result.sound = CaptureReference(perkForm->sound);
            result.swfFile = ReadString(perkForm->swfFile.c_str());
            return result;
        }

        ConstructibleDetails ReadConstructible(const RE::BGSConstructibleObject* constructibleForm, bool advanced)
        {
            ConstructibleDetails result;
            result.createdItem = CaptureReference(constructibleForm->createdItem);
            result.constructedCount = constructibleForm->data.numConstructed;
            if (constructibleForm->requiredItems) for (const auto& item : *constructibleForm->requiredItems) {
                if (result.requirements.size() >= 1024) { result.truncated = true; break; }
                result.requirements.push_back({CaptureReference(item.first), item.second.i});
            }
            if (!advanced) return result;
            result.benchKeyword = CaptureReference(constructibleForm->benchKeyword);
            result.requiredItems = constructibleForm->requiredItems ? constructibleForm->requiredItems->size() : 0;
            result.constructedCount = constructibleForm->data.numConstructed;
            result.workshopPriority = constructibleForm->data.workshopPriority;
            return result;
        }

        QuestDetails ReadQuest(const RE::TESQuest* questForm, bool advanced)
        {
            QuestDetails result;
            result.stages = questForm->stages.size();
            result.currentStage = questForm->currentStage;
            {
                const RE::BSAutoReadLock lock(const_cast<RE::TESQuest*>(questForm)->aliasAccessLock);
                for (const auto* alias : questForm->aliases) {
                    if (result.aliasList.size() >= 1024) { result.truncated = true; break; }
                    if (alias) result.aliasList.push_back({alias->aliasID, ReadString(alias->aliasName.c_str()), alias->flags.underlying()});
                }
            }
            for (const auto* objective : questForm->objectives) {
                if (result.objectiveList.size() >= 1024) { result.truncated = true; break; }
                if (!objective) continue;
                DetailObjective captured{objective->index, ReadString(objective->displayText.c_str()), static_cast<unsigned char>(objective->state)};
                if (objective->targets) for (std::uint32_t index = 0; index < objective->numTargets; ++index) {
                    if (index >= 256) { result.truncated = true; break; }
                    if (const auto* target = objective->targets[index]) captured.aliases.push_back(target->targetAlias);
                }
                result.objectiveList.push_back(std::move(captured));
            }
            if (!advanced) return result;
            result.currentStage = questForm->currentStage;
            result.eventID = questForm->eventID;
            result.stages = questForm->stages.size();
            result.objectives = questForm->objectives.size();
            result.aliases = questForm->aliases.size();
            result.aliasedReferences = questForm->totalRefsAliased;
            result.alreadyRun = questForm->alreadyRun;
            result.delayTime = questForm->data.questDelayTime;
            result.priority = questForm->data.priority;
            result.questType = questForm->data.questType;
            result.flags = questForm->data.flags;
            return result;
        }

        CellDetails ReadCell(const RE::TESObjectCELL* cellForm, bool advanced)
        {
            CellDetails result;
            result.interior = cellForm->IsInterior();
            result.location = CaptureReference(cellForm->GetLocation());
            if (cellForm->IsExterior()) result.worldSpace = CaptureReference(cellForm->worldSpace);
            if (!advanced) return result;
            auto* mutableCell = const_cast<RE::TESObjectCELL*>(cellForm);
            result.hasWater = cellForm->HasWater();
            result.cantWaitHere = mutableCell->GetCantWaitHere();
            result.flags = cellForm->cellFlags.underlying();
            result.gameFlags = cellForm->cellGameFlags;
            result.cellState = std::to_underlying(cellForm->cellState);
            result.x = mutableCell->GetDataX();
            result.y = mutableCell->GetDataY();
            result.encounterZone = CaptureReference(cellForm->GetEncounterZone());
            result.owner = CaptureReference(mutableCell->GetOwner());
            result.waterType = CaptureReference(cellForm->GetWaterType());
            result.waterHeight = cellForm->waterHeight;
            result.lightingTemplate = CaptureReference(cellForm->lightingTemplate);
            result.referenceEntries = static_cast<std::uint32_t>(cellForm->references.size());
            result.exterior = cellForm->IsExterior();
            return result;
        }

    }

    RecordDetails DetailReader::Capture(DetailKey key)
    {
        RecordDetails result;
        result.key = key;
        const auto* form = RE::TESForm::GetFormByID(key.formID);
        if (!form) return result;
        result.status = DetailReadStatus::Available;
        result.editorID = ReadString(form->GetFormEditorID());
        result.type = ReadString(form->GetFormTypeString());
        if (const auto* value = form->As<RE::TESValueForm>()) result.value = value->GetFormValue();
        if (const auto* weight = form->As<RE::TESWeightForm>()) result.weight = weight->GetFormWeight();
        if (const auto* weaponForm = form->As<RE::TESObjectWEAP>()) result.specific = ReadWeapon(weaponForm, key.advanced);
        if (const auto* armorForm = form->As<RE::TESObjectARMO>()) result.specific = ReadArmor(armorForm, key.advanced);
        if (const auto* npcForm = form->As<RE::TESNPC>()) result.specific = ReadNPC(npcForm, key.advanced);
        if (const auto* soundForm = form->As<RE::TESSound>()) result.specific = ReadSound(soundForm, key.advanced);
        if (const auto* ammoForm = form->As<RE::TESAmmo>()) result.specific = ReadAmmo(ammoForm, key.advanced);
        if (const auto* alchemyForm = form->As<RE::AlchemyItem>()) result.specific = ReadAlchemy(alchemyForm, key.advanced);
        if (const auto* globalForm = form->As<RE::TESGlobal>()) result.specific = ReadGlobal(globalForm, key.advanced);
        if (const auto* outfitForm = form->As<RE::BGSOutfit>()) result.specific = ReadOutfit(outfitForm, key.advanced);
        if (const auto* weatherForm = form->As<RE::TESWeather>()) result.specific = ReadWeather(weatherForm, key.advanced);
        if (const auto* activatorForm = form->As<RE::TESObjectACTI>()) result.specific = ReadActivator(activatorForm, key.advanced);
        if (const auto* containerForm = form->As<RE::TESObjectCONT>()) result.specific = ReadContainer(containerForm, key.advanced);
        if (const auto* staticForm = form->As<RE::TESObjectSTAT>()) result.specific = ReadStatic(staticForm, key.advanced);
        if (const auto* furnitureForm = form->As<RE::TESFurniture>()) result.specific = ReadFurniture(furnitureForm, key.advanced);
        if (const auto* spellForm = form->As<RE::SpellItem>()) result.specific = ReadSpell(spellForm, key.advanced);
        if (const auto* perkForm = form->As<RE::BGSPerk>()) result.specific = ReadPerk(perkForm, key.advanced);
        if (const auto* constructibleForm = form->As<RE::BGSConstructibleObject>()) result.specific = ReadConstructible(constructibleForm, key.advanced);
        if (const auto* questForm = form->As<RE::TESQuest>()) result.specific = ReadQuest(questForm, key.advanced);
        if (const auto* cellForm = form->As<RE::TESObjectCELL>()) result.specific = ReadCell(cellForm, key.advanced);
        if (const auto* keywords = form->As<RE::BGSKeywordForm>()) {
            result.keywords.emplace();
            keywords->ForEachKeyword([&](RE::BGSKeyword* keyword) {
                if (result.keywords->size() >= 200) return RE::BSContainer::ForEachResult::kStop;
                if (keyword) result.keywords->push_back(ReadString(keyword->formEditorID.c_str()));
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }
        return result;
    }
}
