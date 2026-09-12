#include "Core/Profiling.h"
#include "GUI/Widgets/FormDetailsView.h"
#include "GUI/Widgets/FormatUtils.h"

#include <imgui.h>
#include <cstdio>

namespace ESPExplorerAE
{
    namespace
    {
        const char* L(const FormDetailsViewContext& context, std::string_view section, std::string_view key)
        {
            return context.localize(section, key, "");
        }
        const char* FD(const FormDetailsViewContext& context, std::string_view key)
        {
            return context.localize("FormDetails", key, "");
        }
        std::string ResolveFormDisplay(const DetailReference& form, const FormDetailsViewContext& context)
        {
            if (!form.formID) return std::string(L(context, "General", "sNone"));
            const auto id = FormatUtils::FormID(form.formID);
            const auto name = !form.name.empty() ? form.name : !form.editorID.empty() ? form.editorID : id;
            if (!form.editorID.empty()) {
                if (!name.empty() && name != form.editorID) return name + " [" + form.editorID + "] (" + id + ")";
                return form.editorID + " (" + id + ")";
            }
            return !name.empty() ? name + " (" + id + ")" : id;
        }
        std::string BuildPluginDisplayName(std::string_view name, const CatalogSnapshot* catalog)
        {
            if (catalog) for (const auto& plugin : catalog->plugins) {
                if (plugin.filename == name && !plugin.formIDPrefix.empty()) return std::string(name) + " [" + plugin.formIDPrefix + "]";
            }
            return std::string(name);
        }

        void DrawCopyPopup(std::string_view value, int& popupCounter, const FormDetailsViewContext& context)
        {
            const std::string popupID = "DetailCopyPopup##" + std::to_string(popupCounter++);
            if (ImGui::BeginPopupContextItem(popupID.c_str())) {
                if (ImGui::MenuItem(L(context, "General", "sCopy"))) {
                    ImGui::SetClipboardText(std::string(value).c_str());
                }
                ImGui::EndPopup();
            }
        }

        void DrawTextLine(const char* label, std::string_view value, int& popupCounter, const FormDetailsViewContext& context)
        {
            ImGui::TextWrapped("%s: %s", label, std::string(value).c_str());
            DrawCopyPopup(value, popupCounter, context);
        }

        void DrawIntLine(const char* label, int value, int& popupCounter, const FormDetailsViewContext& context)
        {
            ImGui::Text("%s: %d", label, value);
            char buffer[32]{};
            std::snprintf(buffer, sizeof(buffer), "%d", value);
            DrawCopyPopup(buffer, popupCounter, context);
        }

        void DrawUIntLine(const char* label, std::uint32_t value, int& popupCounter, const FormDetailsViewContext& context)
        {
            ImGui::Text("%s: %u", label, value);
            char buffer[32]{};
            std::snprintf(buffer, sizeof(buffer), "%u", value);
            DrawCopyPopup(buffer, popupCounter, context);
        }

        void DrawFloatLine(const char* label, float value, int& popupCounter, const FormDetailsViewContext& context)
        {
            ImGui::Text("%s: %.3f", label, value);
            char buffer[48]{};
            std::snprintf(buffer, sizeof(buffer), "%.3f", value);
            DrawCopyPopup(buffer, popupCounter, context);
        }

        void DrawBoolLine(const char* label, bool value, int& popupCounter, const FormDetailsViewContext& context)
        {
            DrawTextLine(label, value ? L(context, "General", "sYes") : L(context, "General", "sNo"), popupCounter, context);
        }

        void DrawFormReferenceLine(const char* label, const DetailReference& form, const FormDetailsViewContext& context, int& popupCounter)
        {
            DrawTextLine(label, ResolveFormDisplay(form, context), popupCounter, context);
        }

        void DrawAdvancedWeaponDetails(const WeaponDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedWeaponData"));

            DrawTextLine(FD(context, "sModel"), details.model, popupCounter, context);
            DrawFormReferenceLine(FD(context, "sEquipmentType"), details.equipmentType, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sBlockBashImpactData"), details.blockBashImpactData, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sBlockBashMaterial"), details.blockBashMaterial, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sInstanceNamingRules"), details.instanceNamingRules, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sImpactDataSet"), details.impactDataSet, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sOnHitEffect"), details.onHitEffect, context, popupCounter);
            DrawFormReferenceLine(L(context, "Items", "sAmmo"), details.ammo, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sSkill"), details.skill, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sResist"), details.resist, context, popupCounter);

            DrawFloatLine(FD(context, "sSpeed"), details.speed, popupCounter, context);
            DrawFloatLine(FD(context, "sReloadSpeed"), details.reloadSpeed, popupCounter, context);
            DrawFloatLine(FD(context, "sReach"), details.reach, popupCounter, context);
            DrawFloatLine(FD(context, "sMinRange"), details.minRange, popupCounter, context);
            DrawFloatLine(FD(context, "sMaxRange"), details.maxRange, popupCounter, context);
            DrawFloatLine(FD(context, "sAttackDelay"), details.attackDelay, popupCounter, context);
            DrawFloatLine(FD(context, "sDamageOutOfRangeMult"), details.damageOutOfRangeMult, popupCounter, context);
            DrawFloatLine(FD(context, "sDamageOnHitMult"), details.damageOnHitMult, popupCounter, context);
            DrawFloatLine(FD(context, "sDamageSecondary"), details.damageSecondary, popupCounter, context);
            DrawFloatLine(L(context, "General", "sWeight"), details.weight, popupCounter, context);
            DrawUIntLine(L(context, "General", "sValue"), details.value, popupCounter, context);
            DrawUIntLine(FD(context, "sDamageBase"), details.damageBase, popupCounter, context);
            DrawFloatLine(FD(context, "sActionPointCost"), details.actionPointCost, popupCounter, context);
            DrawFloatLine(FD(context, "sCriticalChargeBonus"), details.criticalChargeBonus, popupCounter, context);
            DrawFloatLine(FD(context, "sCriticalDamageMult"), details.criticalDamageMult, popupCounter, context);
            DrawFloatLine(FD(context, "sSoundLevelMult"), details.soundLevelMult, popupCounter, context);

            if (details.hasRangedData) {
                DrawFloatLine(FD(context, "sFireSeconds"), details.fireSeconds, popupCounter, context);
                DrawFloatLine(FD(context, "sReloadSeconds"), details.reloadSeconds, popupCounter, context);
                DrawUIntLine(FD(context, "sProjectiles"), details.projectiles, popupCounter, context);
                DrawFormReferenceLine(FD(context, "sOverrideProjectile"), details.overrideProjectile, context, popupCounter);
            }

            if (details.hasDamageTypes) {
                const auto count = static_cast<std::uint32_t>(details.damageTypes.size());
                DrawUIntLine(FD(context, "sDamageTypeEntries"), count, popupCounter, context);
                if (count > 0 && ImGui::CollapsingHeader((std::string(FD(context, "sDamageTypeDetails")) + "###DamageTypeDetails").c_str())) {
                    for (std::uint32_t index = 0; index < count; ++index) {
                        const auto& pair = details.damageTypes[index];
                        const auto key = std::string(FD(context, "sDamageType")) + " " + std::to_string(index + 1);
                        const auto valueKey = std::string(FD(context, "sDamageValue")) + " " + std::to_string(index + 1);
                        DrawFormReferenceLine(key.c_str(), pair.form, context, popupCounter);
                        DrawFloatLine(valueKey.c_str(), pair.value, popupCounter, context);
                    }
                }
            }

            if (details.hasActorValues) {
                const auto count = static_cast<std::uint32_t>(details.actorValues.size());
                DrawUIntLine(FD(context, "sActorValueEntries"), count, popupCounter, context);
                if (count > 0 && ImGui::CollapsingHeader((std::string(FD(context, "sActorValueDetails")) + "###ActorValueDetails").c_str())) {
                    for (std::uint32_t index = 0; index < count; ++index) {
                        const auto& pair = details.actorValues[index];
                        const auto key = std::string(FD(context, "sActorValue")) + " " + std::to_string(index + 1);
                        const auto valueKey = std::string(FD(context, "sActorValueMagnitude")) + " " + std::to_string(index + 1);
                        DrawFormReferenceLine(key.c_str(), pair.form, context, popupCounter);
                        DrawFloatLine(valueKey.c_str(), pair.value, popupCounter, context);
                    }
                }
            }
        }

        void DrawAdvancedArmorDetails(const ArmorDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedArmorData"));

            DrawFormReferenceLine(FD(context, "sEquipmentType"), details.equipmentType, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sBlockBashImpactData"), details.blockBashImpactData, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sBlockBashMaterial"), details.blockBashMaterial, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sInstanceNamingRules"), details.instanceNamingRules, context, popupCounter);

            DrawUIntLine(L(context, "General", "sArmorRating"), details.armorRating, popupCounter, context);
            DrawUIntLine(FD(context, "sHealth"), details.health, popupCounter, context);
            DrawUIntLine(L(context, "General", "sValue"), details.value, popupCounter, context);
            DrawFloatLine(L(context, "General", "sWeight"), details.weight, popupCounter, context);
            DrawFloatLine(FD(context, "sColorRemapIndex"), details.colorRemapIndex, popupCounter, context);

            if (!details.model.empty()) {
                DrawTextLine(FD(context, "sModel"), details.model, popupCounter, context);
            }

            DrawUIntLine(FD(context, "sAddonCount"), details.addonCount, popupCounter, context);

            if (details.hasDamageTypes) {
                DrawUIntLine(FD(context, "sDamageTypeEntries"), details.damageTypeEntries, popupCounter, context);
            }
            if (details.hasActorValues) {
                DrawUIntLine(FD(context, "sActorValueEntries"), details.actorValueEntries, popupCounter, context);
            }
        }

        void DrawAdvancedNPCDetails(const NPCDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedNPCData"));

            DrawFloatLine(FD(context, "sHeight"), details.height, popupCounter, context);
            DrawFloatLine(FD(context, "sHeightMax"), details.heightMax, popupCounter, context);
            DrawFormReferenceLine(FD(context, "sClass"), details.npcClass, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sCombatStyle"), details.combatStyle, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sDefaultOutfit"), details.defaultOutfit, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sSleepOutfit"), details.sleepOutfit, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sCrimeFaction"), details.crimeFaction, context, popupCounter);
        }

        void DrawAdvancedSoundDetails(const SoundDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedSoundData"));

            DrawFormReferenceLine(L(context, "General", "sDescriptor"), details.descriptor, context, popupCounter);
            DrawFloatLine(FD(context, "sMinDelay"), details.minDelay, popupCounter, context);
            DrawFloatLine(FD(context, "sMaxDelay"), details.maxDelay, popupCounter, context);
            DrawBoolLine(FD(context, "sStackable"), details.stackable, popupCounter, context);
        }

        void DrawAdvancedAmmoDetails(const AmmoDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedAmmoData"));

            DrawTextLine(FD(context, "sModel"), details.model, popupCounter, context);
            DrawTextLine(FD(context, "sShellCasingModel"), details.shellCasingModel, popupCounter, context);
            DrawUIntLine(FD(context, "sHealth"), details.health, popupCounter, context);
            DrawUIntLine(FD(context, "sFlags"), details.flags, popupCounter, context);
            DrawFormReferenceLine(FD(context, "sProjectile"), details.projectile, context, popupCounter);
        }

        void DrawAdvancedAlchemyDetails(const AlchemyDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedAlchemyData"));

            DrawTextLine(FD(context, "sModel"), details.model, popupCounter, context);
            DrawFormReferenceLine(FD(context, "sEquipmentType"), details.equipmentType, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sAddictionItem"), details.addictionItem, context, popupCounter);
            DrawFloatLine(FD(context, "sAddictionChance"), details.addictionChance, popupCounter, context);
            DrawFormReferenceLine(FD(context, "sConsumptionSound"), details.consumptionSound, context, popupCounter);
        }

        void DrawAdvancedGlobalDetails(const GlobalDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedGlobalData"));

            DrawFloatLine(L(context, "General", "sValue"), details.value, popupCounter, context);
        }

        void DrawAdvancedOutfitDetails(const OutfitDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedOutfitData"));

            DrawUIntLine(FD(context, "sOutfitItems"), details.outfitItems, popupCounter, context);
        }

        void DrawAdvancedWeatherDetails(const WeatherDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedWeatherData"));

            DrawUIntLine(FD(context, "sFlags"), details.flags, popupCounter, context);
            DrawUIntLine(FD(context, "sCloudLayers"), details.cloudLayers, popupCounter, context);
            DrawUIntLine(FD(context, "sSkyStatics"), details.skyStatics, popupCounter, context);
            DrawFloatLine(FD(context, "sVolatilityMult"), details.volatilityMult, popupCounter, context);
            DrawFloatLine(FD(context, "sVisibilityMult"), details.visibilityMult, popupCounter, context);
            if (!details.auroraModel.empty()) {
                DrawTextLine(FD(context, "sAuroraModel"), details.auroraModel, popupCounter, context);
            }
        }

        void DrawAdvancedActivatorDetails(const ActivatorDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedActivatorData"));

            if (!details.model.empty()) {
                DrawTextLine(FD(context, "sModel"), details.model, popupCounter, context);
            }
            DrawFormReferenceLine(FD(context, "sMaterialSwap"), details.materialSwap, context, popupCounter);
            DrawFloatLine(FD(context, "sColorRemapIndex"), details.colorRemapIndex, popupCounter, context);
            DrawFormReferenceLine(FD(context, "sLoopSound"), details.loopSound, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sActivateSound"), details.activateSound, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sWaterType"), details.waterType, context, popupCounter);
            DrawUIntLine(FD(context, "sFlags"), details.flags, popupCounter, context);
        }

        void DrawAdvancedContainerDetails(const ContainerDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedContainerData"));

            if (!details.model.empty()) {
                DrawTextLine(FD(context, "sModel"), details.model, popupCounter, context);
            }
            DrawFormReferenceLine(FD(context, "sMaterialSwap"), details.materialSwap, context, popupCounter);
            DrawFloatLine(FD(context, "sColorRemapIndex"), details.colorRemapIndex, popupCounter, context);
            DrawFloatLine(L(context, "General", "sWeight"), details.weight, popupCounter, context);
            DrawUIntLine(FD(context, "sFlags"), details.flags, popupCounter, context);
            DrawUIntLine(FD(context, "sContainerEntries"), details.containerEntries, popupCounter, context);
            DrawUIntLine(FD(context, "sContainerItems"), details.containerItems, popupCounter, context);
            DrawFormReferenceLine(FD(context, "sOpenSound"), details.openSound, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sCloseSound"), details.closeSound, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sTakeAllSound"), details.takeAllSound, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sContainsOnlyList"), details.containsOnlyList, context, popupCounter);
        }

        void DrawAdvancedStaticDetails(const StaticDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedStaticData"));

            if (!details.model.empty()) {
                DrawTextLine(FD(context, "sModel"), details.model, popupCounter, context);
            }
            DrawFormReferenceLine(FD(context, "sMaterialSwap"), details.materialSwap, context, popupCounter);
            DrawFloatLine(FD(context, "sColorRemapIndex"), details.colorRemapIndex, popupCounter, context);
            DrawFloatLine(FD(context, "sMaterialThresholdAngle"), details.materialThresholdAngle, popupCounter, context);
            DrawFloatLine(FD(context, "sLeafAmplitude"), details.leafAmplitude, popupCounter, context);
            DrawFloatLine(FD(context, "sLeafFrequency"), details.leafFrequency, popupCounter, context);
        }

        void DrawAdvancedFurnitureDetails(const FurnitureDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedFurnitureData"));

            DrawUIntLine(FD(context, "sEntryPoints"), details.entryPoints, popupCounter, context);
            DrawUIntLine(FD(context, "sMarkerCount"), details.markerCount, popupCounter, context);
            DrawUIntLine(FD(context, "sAttachmentParentCount"), details.attachmentParentCount, popupCounter, context);
            DrawUIntLine(FD(context, "sFlags"), details.flags, popupCounter, context);
            DrawIntLine(FD(context, "sWorkbenchType"), details.workbenchType, popupCounter, context);
            DrawFormReferenceLine(FD(context, "sAssociatedForm"), details.associatedForm, context, popupCounter);
            DrawUIntLine(FD(context, "sContainerEntries"), details.containerEntries, popupCounter, context);
            DrawUIntLine(FD(context, "sContainerItems"), details.containerItems, popupCounter, context);
        }

        void DrawAdvancedSpellDetails(const SpellDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedSpellData"));

            DrawFormReferenceLine(FD(context, "sEquipmentType"), details.equipmentType, context, popupCounter);
            DrawIntLine(FD(context, "sCostOverride"), details.costOverride, popupCounter, context);
            DrawUIntLine(FD(context, "sFlags"), details.flags, popupCounter, context);
            DrawFloatLine(FD(context, "sCastDuration"), details.castDuration, popupCounter, context);
            DrawFloatLine(FD(context, "sRange"), details.range, popupCounter, context);
            DrawFloatLine(FD(context, "sChargeTime"), details.chargeTime, popupCounter, context);
            DrawFormReferenceLine(FD(context, "sCastingPerk"), details.castingPerk, context, popupCounter);
            DrawUIntLine(FD(context, "sEffectEntries"), details.effectEntries, popupCounter, context);
            DrawIntLine(FD(context, "sHostileCount"), details.hostileCount, popupCounter, context);
            DrawUIntLine(FD(context, "sPreloadCount"), details.preloadCount, popupCounter, context);
        }

        void DrawAdvancedPerkDetails(const PerkDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedPerkData"));

            DrawBoolLine(FD(context, "sTrait"), details.trait, popupCounter, context);
            DrawBoolLine(L(context, "General", "sPlayable"), details.playable, popupCounter, context);
            DrawBoolLine(FD(context, "sHidden"), details.hidden, popupCounter, context);
            DrawIntLine(L(context, "General", "sLevel"), details.level, popupCounter, context);
            DrawIntLine(FD(context, "sNumRanks"), details.numRanks, popupCounter, context);
            DrawUIntLine(FD(context, "sPerkEntries"), details.perkEntries, popupCounter, context);
            DrawFormReferenceLine(FD(context, "sNextPerk"), details.nextPerk, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sSound"), details.sound, context, popupCounter);
            if (!details.swfFile.empty()) {
                DrawTextLine(FD(context, "sSWFFile"), details.swfFile, popupCounter, context);
            }
        }

        void DrawAdvancedConstructibleDetails(const ConstructibleDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedConstructibleData"));

            DrawFormReferenceLine(FD(context, "sCreatedItem"), details.createdItem, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sBenchKeyword"), details.benchKeyword, context, popupCounter);
            DrawUIntLine(FD(context, "sRequiredItems"), details.requiredItems, popupCounter, context);
            DrawUIntLine(FD(context, "sConstructedCount"), details.constructedCount, popupCounter, context);
            DrawUIntLine(FD(context, "sWorkshopPriority"), details.workshopPriority, popupCounter, context);
        }

        void DrawAdvancedQuestDetails(const QuestDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedQuestData"));

            DrawUIntLine(FD(context, "sCurrentStage"), details.currentStage, popupCounter, context);
            DrawUIntLine(FD(context, "sEventID"), details.eventID, popupCounter, context);
            DrawUIntLine(FD(context, "sStages"), details.stages, popupCounter, context);
            DrawUIntLine(FD(context, "sObjectives"), details.objectives, popupCounter, context);
            DrawUIntLine(FD(context, "sAliases"), details.aliases, popupCounter, context);
            DrawUIntLine(FD(context, "sAliasedReferences"), details.aliasedReferences, popupCounter, context);
            DrawBoolLine(FD(context, "sAlreadyRun"), details.alreadyRun, popupCounter, context);
            DrawFloatLine(FD(context, "sDelayTime"), details.delayTime, popupCounter, context);
            DrawIntLine(FD(context, "sPriority"), details.priority, popupCounter, context);
            DrawIntLine(FD(context, "sQuestType"), details.questType, popupCounter, context);
            DrawUIntLine(FD(context, "sFlags"), details.flags, popupCounter, context);
        }

        void DrawAdvancedCellDetails(const CellDetails& details, const FormDetailsViewContext& context, int& popupCounter)
        {

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedCellData"));

            DrawBoolLine(FD(context, "sInterior"), details.interior, popupCounter, context);
            DrawBoolLine(FD(context, "sHasWater"), details.hasWater, popupCounter, context);
            DrawBoolLine(FD(context, "sCantWaitHere"), details.cantWaitHere, popupCounter, context);
            DrawUIntLine(FD(context, "sFlags"), details.flags, popupCounter, context);
            DrawUIntLine(FD(context, "sGameFlags"), details.gameFlags, popupCounter, context);
            DrawUIntLine(FD(context, "sCellState"), details.cellState, popupCounter, context);
            DrawIntLine(FD(context, "sX"), details.x, popupCounter, context);
            DrawIntLine(FD(context, "sY"), details.y, popupCounter, context);
            DrawFormReferenceLine(FD(context, "sEncounterZone"), details.encounterZone, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sLocation"), details.location, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sOwner"), details.owner, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sWaterType"), details.waterType, context, popupCounter);
            DrawFloatLine(FD(context, "sWaterHeight"), details.waterHeight, popupCounter, context);
            DrawFormReferenceLine(FD(context, "sLightingTemplate"), details.lightingTemplate, context, popupCounter);
            DrawUIntLine(FD(context, "sReferenceEntries"), details.referenceEntries, popupCounter, context);
            if (details.exterior) {
                DrawFormReferenceLine(FD(context, "sWorldSpace"), details.worldSpace, context, popupCounter);
            }
        }

        void DrawKeywordDetails(const std::optional<std::vector<std::string>>& keywords, const FormDetailsViewContext& context)
        {
            if (!keywords) return;
            ImGui::Separator();
            ImGui::TextUnformatted(L(context, "General", "sKeywords"));
            for (std::size_t index = 0; index < keywords->size(); ++index) {
                const auto& text = (*keywords)[index];
                const auto* label = text.empty() ? L(context, "General", "sUnnamedKeyword") : text.c_str();
                ImGui::BulletText("%s", label);
                const auto id = "KeywordCopyPopup##" + std::to_string(index);
                if (ImGui::BeginPopupContextItem(id.c_str())) {
                    if (ImGui::MenuItem(L(context, "General", "sCopy"))) ImGui::SetClipboardText(label);
                    ImGui::EndPopup();
                }
            }
            if (keywords->empty()) ImGui::TextDisabled("%s", L(context, "General", "sNoKeywords"));
        }
    }

    void FormDetailsView::Draw(const FormEntry& selectedRecord, const FormDetailsViewContext& context)
    {
        const ProfileScope profileScope(ProfileMetric::DetailsView);
        ImGui::PushID(static_cast<int>(selectedRecord.formID));
        int detailCopyPopupCounter = 0;
        ImGui::TextWrapped("%s", selectedRecord.name.empty() ? L(context, "General", "sUnnamed") : selectedRecord.name.c_str());
        DrawCopyPopup(selectedRecord.name.empty() ? L(context, "General", "sUnnamed") : selectedRecord.name, detailCopyPopupCounter, context);
        const auto id = FormatUtils::FormID(selectedRecord.formID);
        ImGui::TextDisabled("%s", id.c_str());
        DrawCopyPopup(id, detailCopyPopupCounter, context);
        ImGui::Separator();
        DrawTextLine(L(context, "General", "sPlugin"), BuildPluginDisplayName(selectedRecord.sourcePlugin, context.catalog), detailCopyPopupCounter, context);
        DrawTextLine(L(context, "General", "sCategory"), selectedRecord.category, detailCopyPopupCounter, context);
        DrawBoolLine(L(context, "General", "sPlayable"), selectedRecord.isPlayable, detailCopyPopupCounter, context);
        DrawBoolLine(L(context, "General", "sDeleted"), selectedRecord.isDeleted, detailCopyPopupCounter, context);
        if (!context.details || context.details->key.formID != selectedRecord.formID ||
            (context.catalog && context.details->key.catalogGeneration != context.catalog->generation) ||
            context.details->key.advanced != context.showAdvancedDetailsView) {
            ImGui::TextDisabled("%s", context.localize("FormDetails", "sLoading", "Loading record details..."));
            ImGui::PopID();
            return;
        }
        const auto& details = *context.details;
        if (details.status != DetailReadStatus::Available) {
            ImGui::TextDisabled("%s", context.localize("FormDetails", "sUnavailable", "Record details are unavailable."));
            ImGui::PopID();
            return;
        }
        DrawTextLine(L(context, "General", "sEditorID"), details.editorID.empty() ? L(context, "General", "sNone") : details.editorID, detailCopyPopupCounter, context);
        DrawTextLine(L(context, "General", "sType"), details.type, detailCopyPopupCounter, context);
        std::uint32_t referenceCount{};
        if (context.catalog) {
            const auto found = context.catalog->runtimeReferenceCounts.find(selectedRecord.formID);
            if (found != context.catalog->runtimeReferenceCounts.end()) referenceCount = found->second;
        }
        DrawUIntLine(context.localize("FormDetails", "sRuntimeReferenceCount", "Runtime-known references"), referenceCount, detailCopyPopupCounter, context);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", context.localize("FormDetails", "sRuntimeReferenceScope",
            "References currently registered with the game; not all placed references in plugin files."));
        if (details.value) DrawIntLine(L(context, "General", "sValue"), *details.value, detailCopyPopupCounter, context);
        if (details.weight) DrawFloatLine(L(context, "General", "sWeight"), *details.weight, detailCopyPopupCounter, context);
        if (const auto* weapon = std::get_if<WeaponDetails>(&details.specific)) {
            DrawUIntLine(L(context, "General", "sDamage"), weapon->damageBase, detailCopyPopupCounter, context);
            if (weapon->fireRate) DrawFloatLine(L(context, "General", "sFireRate"), *weapon->fireRate, detailCopyPopupCounter, context);
            DrawFormReferenceLine(L(context, "Items", "sAmmo"), weapon->ammo, context, detailCopyPopupCounter);
            DrawUIntLine(FD(context, "sAmmoCapacity"), weapon->ammoCapacity, detailCopyPopupCounter, context);
            DrawUIntLine(FD(context, "sAttachmentParentCount"), weapon->attachmentParentCount, detailCopyPopupCounter, context);
        }
        if (const auto* ammo = std::get_if<AmmoDetails>(&details.specific)) DrawFloatLine(L(context, "General", "sDamage"), ammo->damage, detailCopyPopupCounter, context);
        if (const auto* armor = std::get_if<ArmorDetails>(&details.specific)) {
            DrawUIntLine(L(context, "General", "sArmorRating"), armor->armorRating, detailCopyPopupCounter, context);
            DrawUIntLine(FD(context, "sAttachmentParentCount"), armor->attachmentParentCount, detailCopyPopupCounter, context);
        }
        if (const auto* npc = std::get_if<NPCDetails>(&details.specific)) {
            DrawIntLine(L(context, "General", "sLevel"), npc->level, detailCopyPopupCounter, context);
            DrawFormReferenceLine(L(context, "NPCs", "sResolvedRace"), npc->resolvedRace, context, detailCopyPopupCounter);
            DrawUIntLine(FD(context, "sAttachmentParentCount"), npc->attachmentParentCount, detailCopyPopupCounter, context);
        }
        if (const auto* sound = std::get_if<SoundDetails>(&details.specific)) DrawFormReferenceLine(L(context, "General", "sDescriptor"), sound->descriptor, context, detailCopyPopupCounter);
        if (context.showAdvancedDetailsView) {
            if (const auto* value = std::get_if<WeaponDetails>(&details.specific)) DrawAdvancedWeaponDetails(*value, context, detailCopyPopupCounter);
            if (const auto* value = std::get_if<ArmorDetails>(&details.specific)) DrawAdvancedArmorDetails(*value, context, detailCopyPopupCounter);
            if (const auto* value = std::get_if<NPCDetails>(&details.specific)) DrawAdvancedNPCDetails(*value, context, detailCopyPopupCounter);
            if (const auto* value = std::get_if<SoundDetails>(&details.specific)) DrawAdvancedSoundDetails(*value, context, detailCopyPopupCounter);
            if (const auto* value = std::get_if<GlobalDetails>(&details.specific)) DrawAdvancedGlobalDetails(*value, context, detailCopyPopupCounter);
            if (const auto* value = std::get_if<OutfitDetails>(&details.specific)) DrawAdvancedOutfitDetails(*value, context, detailCopyPopupCounter);
            if (const auto* value = std::get_if<WeatherDetails>(&details.specific)) DrawAdvancedWeatherDetails(*value, context, detailCopyPopupCounter);
            if (const auto* value = std::get_if<ActivatorDetails>(&details.specific)) DrawAdvancedActivatorDetails(*value, context, detailCopyPopupCounter);
            if (const auto* value = std::get_if<ContainerDetails>(&details.specific)) DrawAdvancedContainerDetails(*value, context, detailCopyPopupCounter);
            if (const auto* value = std::get_if<StaticDetails>(&details.specific)) DrawAdvancedStaticDetails(*value, context, detailCopyPopupCounter);
            if (const auto* value = std::get_if<FurnitureDetails>(&details.specific)) DrawAdvancedFurnitureDetails(*value, context, detailCopyPopupCounter);
            if (const auto* value = std::get_if<SpellDetails>(&details.specific)) DrawAdvancedSpellDetails(*value, context, detailCopyPopupCounter);
            if (const auto* value = std::get_if<PerkDetails>(&details.specific)) DrawAdvancedPerkDetails(*value, context, detailCopyPopupCounter);
            if (const auto* value = std::get_if<AmmoDetails>(&details.specific)) DrawAdvancedAmmoDetails(*value, context, detailCopyPopupCounter);
            if (const auto* value = std::get_if<AlchemyDetails>(&details.specific)) DrawAdvancedAlchemyDetails(*value, context, detailCopyPopupCounter);
            if (const auto* value = std::get_if<ConstructibleDetails>(&details.specific)) DrawAdvancedConstructibleDetails(*value, context, detailCopyPopupCounter);
            if (const auto* value = std::get_if<QuestDetails>(&details.specific)) DrawAdvancedQuestDetails(*value, context, detailCopyPopupCounter);
            if (const auto* value = std::get_if<CellDetails>(&details.specific)) DrawAdvancedCellDetails(*value, context, detailCopyPopupCounter);
        }
        DrawKeywordDetails(details.keywords, context);
        ImGui::PopID();
    }
}
