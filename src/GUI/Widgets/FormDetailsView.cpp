#include "GUI/Widgets/FormDetailsView.h"

#include "Data/DataManager.h"

#include <imgui.h>

#include <RE/A/AlchemyItem.h>
#include <RE/B/BGSConstructibleObject.h>
#include <RE/B/BGSEncounterZone.h>
#include <RE/B/BGSKeywordForm.h>
#include <RE/B/BGSLightingTemplate.h>
#include <RE/B/BGSLocation.h>
#include <RE/B/BGSOutfit.h>
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

        std::string ResolveFormName(const RE::TESForm* form)
        {
            if (!form) {
                return {};
            }

            const auto fullName = RE::TESFullName::GetFullName(*form);
            if (!fullName.empty()) {
                return std::string(fullName);
            }

            const auto* editorID = form->GetFormEditorID();
            if (editorID && editorID[0] != '\0') {
                return std::string(editorID);
            }

            char buffer[16]{};
            std::snprintf(buffer, sizeof(buffer), "%08X", form->GetFormID());
            return std::string(buffer);
        }

        std::string ResolveFormDisplay(const RE::TESForm* form, const FormDetailsViewContext& context)
        {
            if (!form) {
                return std::string(L(context, "General", "sNone"));
            }

            const auto name = ResolveFormName(form);
            const auto* editorID = form->GetFormEditorID();

            char idBuffer[16]{};
            std::snprintf(idBuffer, sizeof(idBuffer), "%08X", form->GetFormID());

            if (editorID && editorID[0] != '\0') {
                if (!name.empty() && name != editorID) {
                    return name + " [" + editorID + "] (" + idBuffer + ")";
                }
                return std::string(editorID) + " (" + idBuffer + ")";
            }

            if (!name.empty()) {
                return name + " (" + idBuffer + ")";
            }

            return std::string(idBuffer);
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

        std::string BuildPluginDisplayName(std::string_view pluginName)
        {
            if (pluginName.empty()) {
                return {};
            }

            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) {
                return std::string(pluginName);
            }

            if (const auto* file = dataHandler->LookupLoadedModByName(pluginName)) {
                return std::string(pluginName) + " [" + FormatPluginFormIDPrefix(*file) + "]";
            }

            if (const auto* file = dataHandler->LookupLoadedLightModByName(pluginName)) {
                return std::string(pluginName) + " [" + FormatPluginFormIDPrefix(*file) + "]";
            }

            return std::string(pluginName);
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
            ImGui::Text("%s: %s", label, std::string(value).c_str());
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

        void DrawFormReferenceLine(const char* label, const RE::TESForm* form, const FormDetailsViewContext& context, int& popupCounter)
        {
            DrawTextLine(label, ResolveFormDisplay(form, context), popupCounter, context);
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

        void DrawAdvancedWeaponDetails(const RE::TESObjectWEAP* weaponForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!weaponForm) {
                return;
            }

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedWeaponData"));

            DrawTextLine(FD(context, "sModel"), weaponForm->model.c_str(), popupCounter, context);
            DrawFormReferenceLine(FD(context, "sEquipmentType"), weaponForm->GetEquipSlot(nullptr), context, popupCounter);
            DrawFormReferenceLine(FD(context, "sBlockBashImpactData"), weaponForm->blockBashImpactDataSet, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sBlockBashMaterial"), weaponForm->altBlockMaterialType, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sInstanceNamingRules"), weaponForm->instanceNamingRules, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sImpactDataSet"), weaponForm->weaponData.impactDataSet, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sOnHitEffect"), weaponForm->weaponData.effect, context, popupCounter);
            DrawFormReferenceLine(L(context, "Items", "sAmmo"), weaponForm->weaponData.ammo, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sSkill"), weaponForm->weaponData.skill, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sResist"), weaponForm->weaponData.resistance, context, popupCounter);

            DrawFloatLine(FD(context, "sSpeed"), weaponForm->weaponData.speed, popupCounter, context);
            DrawFloatLine(FD(context, "sReloadSpeed"), weaponForm->weaponData.reloadSpeed, popupCounter, context);
            DrawFloatLine(FD(context, "sReach"), weaponForm->weaponData.reach, popupCounter, context);
            DrawFloatLine(FD(context, "sMinRange"), weaponForm->weaponData.minRange, popupCounter, context);
            DrawFloatLine(FD(context, "sMaxRange"), weaponForm->weaponData.maxRange, popupCounter, context);
            DrawFloatLine(FD(context, "sAttackDelay"), weaponForm->weaponData.attackDelaySec, popupCounter, context);
            DrawFloatLine(FD(context, "sDamageOutOfRangeMult"), weaponForm->weaponData.outOfRangeDamageMult, popupCounter, context);
            DrawFloatLine(FD(context, "sDamageOnHitMult"), weaponForm->weaponData.damageToWeaponMult, popupCounter, context);
            DrawFloatLine(FD(context, "sDamageSecondary"), weaponForm->weaponData.secondaryDamage, popupCounter, context);
            DrawFloatLine(L(context, "General", "sWeight"), weaponForm->weaponData.weight, popupCounter, context);
            DrawUIntLine(L(context, "General", "sValue"), weaponForm->weaponData.value, popupCounter, context);
            DrawUIntLine(FD(context, "sDamageBase"), weaponForm->weaponData.attackDamage, popupCounter, context);
            DrawFloatLine(FD(context, "sActionPointCost"), weaponForm->weaponData.attackActionPointCost, popupCounter, context);
            DrawFloatLine(FD(context, "sCriticalChargeBonus"), weaponForm->weaponData.criticalChargeBonus, popupCounter, context);
            DrawFloatLine(FD(context, "sCriticalDamageMult"), weaponForm->weaponData.criticalDamageMult, popupCounter, context);
            DrawFloatLine(FD(context, "sSoundLevelMult"), weaponForm->weaponData.soundLevelMult, popupCounter, context);

            if (weaponForm->weaponData.rangedData) {
                DrawFloatLine(FD(context, "sFireSeconds"), weaponForm->weaponData.rangedData->fireSeconds, popupCounter, context);
                DrawFloatLine(FD(context, "sReloadSeconds"), weaponForm->weaponData.rangedData->reloadSeconds, popupCounter, context);
                DrawUIntLine(FD(context, "sProjectiles"), static_cast<std::uint32_t>(weaponForm->weaponData.rangedData->numProjectiles), popupCounter, context);
                DrawFormReferenceLine(FD(context, "sOverrideProjectile"), weaponForm->weaponData.rangedData->overrideProjectile, context, popupCounter);
            }

            if (weaponForm->weaponData.damageTypes) {
                const auto count = weaponForm->weaponData.damageTypes->size();
                DrawUIntLine(FD(context, "sDamageTypeEntries"), count, popupCounter, context);
                if (count > 0 && ImGui::CollapsingHeader(FD(context, "sDamageTypeDetails"))) {
                    for (std::uint32_t index = 0; index < count; ++index) {
                        const auto& pair = (*weaponForm->weaponData.damageTypes)[index];
                        const auto key = std::string(FD(context, "sDamageType")) + " " + std::to_string(index + 1);
                        const auto valueKey = std::string(FD(context, "sDamageValue")) + " " + std::to_string(index + 1);
                        DrawFormReferenceLine(key.c_str(), pair.first, context, popupCounter);
                        DrawFloatLine(valueKey.c_str(), pair.second.f, popupCounter, context);
                    }
                }
            }

            if (weaponForm->weaponData.actorValues) {
                const auto count = weaponForm->weaponData.actorValues->size();
                DrawUIntLine(FD(context, "sActorValueEntries"), count, popupCounter, context);
                if (count > 0 && ImGui::CollapsingHeader(FD(context, "sActorValueDetails"))) {
                    for (std::uint32_t index = 0; index < count; ++index) {
                        const auto& pair = (*weaponForm->weaponData.actorValues)[index];
                        const auto key = std::string(FD(context, "sActorValue")) + " " + std::to_string(index + 1);
                        const auto valueKey = std::string(FD(context, "sActorValueMagnitude")) + " " + std::to_string(index + 1);
                        DrawFormReferenceLine(key.c_str(), pair.first, context, popupCounter);
                        DrawFloatLine(valueKey.c_str(), pair.second.f, popupCounter, context);
                    }
                }
            }
        }

        void DrawAdvancedArmorDetails(const RE::TESObjectARMO* armorForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!armorForm) {
                return;
            }

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedArmorData"));

            DrawFormReferenceLine(FD(context, "sEquipmentType"), armorForm->GetEquipSlot(nullptr), context, popupCounter);
            DrawFormReferenceLine(FD(context, "sBlockBashImpactData"), armorForm->blockBashImpactDataSet, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sBlockBashMaterial"), armorForm->altBlockMaterialType, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sInstanceNamingRules"), armorForm->instanceNamingRules, context, popupCounter);

            DrawUIntLine(L(context, "General", "sArmorRating"), armorForm->armorData.rating, popupCounter, context);
            DrawUIntLine(FD(context, "sHealth"), armorForm->armorData.health, popupCounter, context);
            DrawUIntLine(L(context, "General", "sValue"), armorForm->armorData.value, popupCounter, context);
            DrawFloatLine(L(context, "General", "sWeight"), armorForm->armorData.weight, popupCounter, context);
            DrawFloatLine(FD(context, "sColorRemapIndex"), armorForm->armorData.colorRemappingIndex, popupCounter, context);

            if (!armorForm->worldModel[0].model.empty()) {
                DrawTextLine(FD(context, "sModel"), armorForm->worldModel[0].model.c_str(), popupCounter, context);
            }

            DrawUIntLine(FD(context, "sAddonCount"), armorForm->modelArray.size(), popupCounter, context);

            if (armorForm->armorData.damageTypes) {
                DrawUIntLine(FD(context, "sDamageTypeEntries"), armorForm->armorData.damageTypes->size(), popupCounter, context);
            }
            if (armorForm->armorData.actorValues) {
                DrawUIntLine(FD(context, "sActorValueEntries"), armorForm->armorData.actorValues->size(), popupCounter, context);
            }
        }

        void DrawAdvancedNPCDetails(const RE::TESNPC* npcForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!npcForm) {
                return;
            }

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedNPCData"));

            DrawFloatLine(FD(context, "sHeight"), npcForm->height, popupCounter, context);
            DrawFloatLine(FD(context, "sHeightMax"), npcForm->heightMax, popupCounter, context);
            DrawFormReferenceLine(FD(context, "sClass"), npcForm->cl, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sCombatStyle"), npcForm->combatStyle, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sDefaultOutfit"), npcForm->defOutfit, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sSleepOutfit"), npcForm->sleepOutfit, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sCrimeFaction"), npcForm->crimeFaction, context, popupCounter);
        }

        void DrawAdvancedSoundDetails(const RE::TESSound* soundForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!soundForm) {
                return;
            }

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedSoundData"));

            DrawFormReferenceLine(L(context, "General", "sDescriptor"), soundForm->descriptor, context, popupCounter);
            DrawFloatLine(FD(context, "sMinDelay"), soundForm->repeatData.minDelay, popupCounter, context);
            DrawFloatLine(FD(context, "sMaxDelay"), soundForm->repeatData.maxDelay, popupCounter, context);
            DrawTextLine(FD(context, "sStackable"), soundForm->repeatData.stackable ? L(context, "General", "sYes") : L(context, "General", "sNo"), popupCounter, context);
        }

        void DrawAdvancedAmmoDetails(const RE::TESAmmo* ammoForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!ammoForm) {
                return;
            }

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedAmmoData"));

            DrawTextLine(FD(context, "sModel"), ammoForm->model.c_str(), popupCounter, context);
            DrawTextLine(FD(context, "sShellCasingModel"), ammoForm->shellCasing.model.c_str(), popupCounter, context);
            DrawUIntLine(FD(context, "sHealth"), ammoForm->data.health, popupCounter, context);
            DrawUIntLine(FD(context, "sFlags"), static_cast<std::uint32_t>(static_cast<unsigned char>(ammoForm->data.flags)), popupCounter, context);
            DrawFormReferenceLine(FD(context, "sProjectile"), ammoForm->data.projectile, context, popupCounter);
        }

        void DrawAdvancedAlchemyDetails(const RE::AlchemyItem* alchemyForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!alchemyForm) {
                return;
            }

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedAlchemyData"));

            DrawTextLine(FD(context, "sModel"), alchemyForm->model.c_str(), popupCounter, context);
            DrawFormReferenceLine(FD(context, "sEquipmentType"), alchemyForm->GetEquipSlot(nullptr), context, popupCounter);
            DrawFormReferenceLine(FD(context, "sAddictionItem"), alchemyForm->data.addictionItem, context, popupCounter);
            DrawFloatLine(FD(context, "sAddictionChance"), alchemyForm->data.addictionChance, popupCounter, context);
            DrawFormReferenceLine(FD(context, "sConsumptionSound"), alchemyForm->data.consumptionSound, context, popupCounter);
        }

        void DrawAdvancedGlobalDetails(const RE::TESGlobal* globalForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!globalForm) {
                return;
            }

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedGlobalData"));

            DrawFloatLine(L(context, "General", "sValue"), globalForm->GetValue(), popupCounter, context);
        }

        void DrawAdvancedOutfitDetails(const RE::BGSOutfit* outfitForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!outfitForm) {
                return;
            }

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedOutfitData"));

            DrawUIntLine(FD(context, "sOutfitItems"), static_cast<std::uint32_t>(outfitForm->outfitItems.size()), popupCounter, context);
        }

        void DrawAdvancedWeatherDetails(const RE::TESWeather* weatherForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!weatherForm) {
                return;
            }

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedWeatherData"));

            DrawUIntLine(FD(context, "sFlags"), static_cast<std::uint32_t>(static_cast<unsigned char>(weatherForm->weatherData[static_cast<std::size_t>(RE::TESWeather::WeatherData::kFlags)])), popupCounter, context);
            DrawUIntLine(FD(context, "sCloudLayers"), weatherForm->numCloudLayers, popupCounter, context);
            DrawUIntLine(FD(context, "sSkyStatics"), static_cast<std::uint32_t>(weatherForm->skyStatics.size()), popupCounter, context);
            DrawFloatLine(FD(context, "sVolatilityMult"), weatherForm->volatilityMult, popupCounter, context);
            DrawFloatLine(FD(context, "sVisibilityMult"), weatherForm->visibilityMult, popupCounter, context);
            if (!weatherForm->aurora.model.empty()) {
                DrawTextLine(FD(context, "sAuroraModel"), weatherForm->aurora.model.c_str(), popupCounter, context);
            }
        }

        void DrawAdvancedActivatorDetails(const RE::TESObjectACTI* activatorForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!activatorForm) {
                return;
            }

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedActivatorData"));

            if (!activatorForm->model.empty()) {
                DrawTextLine(FD(context, "sModel"), activatorForm->model.c_str(), popupCounter, context);
            }
            DrawFormReferenceLine(FD(context, "sMaterialSwap"), activatorForm->swapForm, context, popupCounter);
            DrawFloatLine(FD(context, "sColorRemapIndex"), activatorForm->colorRemappingIndex, popupCounter, context);
            DrawFormReferenceLine(FD(context, "sLoopSound"), activatorForm->soundLoop, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sActivateSound"), activatorForm->soundActivate, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sWaterType"), activatorForm->waterForm, context, popupCounter);
            DrawUIntLine(FD(context, "sFlags"), activatorForm->flags, popupCounter, context);
        }

        void DrawAdvancedContainerDetails(const RE::TESObjectCONT* containerForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!containerForm) {
                return;
            }

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedContainerData"));

            if (!containerForm->model.empty()) {
                DrawTextLine(FD(context, "sModel"), containerForm->model.c_str(), popupCounter, context);
            }
            DrawFormReferenceLine(FD(context, "sMaterialSwap"), containerForm->swapForm, context, popupCounter);
            DrawFloatLine(FD(context, "sColorRemapIndex"), containerForm->colorRemappingIndex, popupCounter, context);
            DrawFloatLine(L(context, "General", "sWeight"), containerForm->weight, popupCounter, context);
            DrawUIntLine(FD(context, "sFlags"), static_cast<std::uint32_t>(static_cast<unsigned char>(containerForm->data.contFlags)), popupCounter, context);
            DrawUIntLine(FD(context, "sContainerEntries"), CountContainerEntries(containerForm), popupCounter, context);
            DrawUIntLine(FD(context, "sContainerItems"), CountContainerItems(containerForm), popupCounter, context);
            DrawFormReferenceLine(FD(context, "sOpenSound"), containerForm->openSound, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sCloseSound"), containerForm->closeSound, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sTakeAllSound"), containerForm->takeAllSound, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sContainsOnlyList"), containerForm->containsOnlyList, context, popupCounter);
        }

        void DrawAdvancedStaticDetails(const RE::TESObjectSTAT* staticForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!staticForm) {
                return;
            }

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedStaticData"));

            if (!staticForm->model.empty()) {
                DrawTextLine(FD(context, "sModel"), staticForm->model.c_str(), popupCounter, context);
            }
            DrawFormReferenceLine(FD(context, "sMaterialSwap"), staticForm->swapForm, context, popupCounter);
            DrawFloatLine(FD(context, "sColorRemapIndex"), staticForm->colorRemappingIndex, popupCounter, context);
            DrawFloatLine(FD(context, "sMaterialThresholdAngle"), staticForm->data.materialThresholdAngle, popupCounter, context);
            DrawFloatLine(FD(context, "sLeafAmplitude"), staticForm->data.leafAmplitude, popupCounter, context);
            DrawFloatLine(FD(context, "sLeafFrequency"), staticForm->data.leafFrequency, popupCounter, context);
        }

        void DrawAdvancedFurnitureDetails(const RE::TESFurniture* furnitureForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!furnitureForm) {
                return;
            }

            auto* mutableFurniture = const_cast<RE::TESFurniture*>(furnitureForm);
            auto* container = mutableFurniture->GetContainer();

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedFurnitureData"));

            DrawUIntLine(FD(context, "sEntryPoints"), static_cast<std::uint32_t>(furnitureForm->entryPointDataArray.size()), popupCounter, context);
            DrawUIntLine(FD(context, "sMarkerCount"), static_cast<std::uint32_t>(furnitureForm->markersArray.size()), popupCounter, context);
            DrawUIntLine(FD(context, "sAttachmentParentCount"), furnitureForm->attachParents.size, popupCounter, context);
            DrawUIntLine(FD(context, "sFlags"), furnitureForm->furnFlags, popupCounter, context);
            DrawIntLine(FD(context, "sWorkbenchType"), furnitureForm->wbData.type.underlying(), popupCounter, context);
            DrawFormReferenceLine(FD(context, "sAssociatedForm"), furnitureForm->associatedForm, context, popupCounter);
            DrawUIntLine(FD(context, "sContainerEntries"), CountContainerEntries(container), popupCounter, context);
            DrawUIntLine(FD(context, "sContainerItems"), CountContainerItems(container), popupCounter, context);
        }

        void DrawAdvancedSpellDetails(const RE::SpellItem* spellForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!spellForm) {
                return;
            }

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedSpellData"));

            DrawFormReferenceLine(FD(context, "sEquipmentType"), spellForm->GetEquipSlot(nullptr), context, popupCounter);
            DrawIntLine(FD(context, "sCostOverride"), spellForm->data.costOverride, popupCounter, context);
            DrawUIntLine(FD(context, "sFlags"), spellForm->data.flags, popupCounter, context);
            DrawFloatLine(FD(context, "sCastDuration"), spellForm->data.castDuration, popupCounter, context);
            DrawFloatLine(FD(context, "sRange"), spellForm->data.range, popupCounter, context);
            DrawFloatLine(FD(context, "sChargeTime"), spellForm->data.chargeTime, popupCounter, context);
            DrawFormReferenceLine(FD(context, "sCastingPerk"), spellForm->data.castingPerk, context, popupCounter);
            DrawUIntLine(FD(context, "sEffectEntries"), static_cast<std::uint32_t>(spellForm->listOfEffects.size()), popupCounter, context);
            DrawIntLine(FD(context, "sHostileCount"), spellForm->hostileCount, popupCounter, context);
            DrawUIntLine(FD(context, "sPreloadCount"), spellForm->preloadCount, popupCounter, context);
        }

        void DrawAdvancedPerkDetails(const RE::BGSPerk* perkForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!perkForm) {
                return;
            }

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedPerkData"));

            DrawBoolLine(FD(context, "sTrait"), perkForm->data.trait, popupCounter, context);
            DrawBoolLine(L(context, "General", "sPlayable"), perkForm->data.playable, popupCounter, context);
            DrawBoolLine(FD(context, "sHidden"), perkForm->data.hidden, popupCounter, context);
            DrawIntLine(L(context, "General", "sLevel"), perkForm->data.level, popupCounter, context);
            DrawIntLine(FD(context, "sNumRanks"), perkForm->data.numRanks, popupCounter, context);
            DrawUIntLine(FD(context, "sPerkEntries"), static_cast<std::uint32_t>(perkForm->perkEntries.size()), popupCounter, context);
            DrawFormReferenceLine(FD(context, "sNextPerk"), perkForm->nextPerk, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sSound"), perkForm->sound, context, popupCounter);
            if (!perkForm->swfFile.empty()) {
                DrawTextLine(FD(context, "sSWFFile"), perkForm->swfFile.c_str(), popupCounter, context);
            }
        }

        void DrawAdvancedConstructibleDetails(const RE::BGSConstructibleObject* constructibleForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!constructibleForm) {
                return;
            }

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedConstructibleData"));

            DrawFormReferenceLine(FD(context, "sCreatedItem"), constructibleForm->createdItem, context, popupCounter);
            DrawFormReferenceLine(FD(context, "sBenchKeyword"), constructibleForm->benchKeyword, context, popupCounter);
            DrawUIntLine(FD(context, "sRequiredItems"), constructibleForm->requiredItems ? constructibleForm->requiredItems->size() : 0, popupCounter, context);
            DrawUIntLine(FD(context, "sConstructedCount"), constructibleForm->data.numConstructed, popupCounter, context);
            DrawUIntLine(FD(context, "sWorkshopPriority"), constructibleForm->data.workshopPriority, popupCounter, context);
        }

        void DrawAdvancedQuestDetails(const RE::TESQuest* questForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!questForm) {
                return;
            }

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedQuestData"));

            DrawUIntLine(FD(context, "sCurrentStage"), questForm->currentStage, popupCounter, context);
            DrawUIntLine(FD(context, "sEventID"), questForm->eventID, popupCounter, context);
            DrawUIntLine(FD(context, "sStages"), questForm->stages.size(), popupCounter, context);
            DrawUIntLine(FD(context, "sObjectives"), questForm->objectives.size(), popupCounter, context);
            DrawUIntLine(FD(context, "sAliases"), questForm->aliases.size(), popupCounter, context);
            DrawUIntLine(FD(context, "sAliasedReferences"), questForm->totalRefsAliased, popupCounter, context);
            DrawTextLine(FD(context, "sAlreadyRun"), questForm->alreadyRun ? L(context, "General", "sYes") : L(context, "General", "sNo"), popupCounter, context);
            DrawFloatLine(FD(context, "sDelayTime"), questForm->data.questDelayTime, popupCounter, context);
            DrawIntLine(FD(context, "sPriority"), questForm->data.priority, popupCounter, context);
            DrawIntLine(FD(context, "sQuestType"), questForm->data.questType, popupCounter, context);
            DrawUIntLine(FD(context, "sFlags"), questForm->data.flags, popupCounter, context);
        }

        void DrawAdvancedCellDetails(const RE::TESObjectCELL* cellForm, const FormDetailsViewContext& context, int& popupCounter)
        {
            if (!cellForm) {
                return;
            }

            auto* mutableCell = const_cast<RE::TESObjectCELL*>(cellForm);

            ImGui::Separator();
            ImGui::TextUnformatted(FD(context, "sAdvancedCellData"));

            DrawBoolLine(FD(context, "sInterior"), cellForm->IsInterior(), popupCounter, context);
            DrawBoolLine(FD(context, "sHasWater"), cellForm->HasWater(), popupCounter, context);
            DrawBoolLine(FD(context, "sCantWaitHere"), mutableCell->GetCantWaitHere(), popupCounter, context);
            DrawUIntLine(FD(context, "sFlags"), cellForm->cellFlags.underlying(), popupCounter, context);
            DrawUIntLine(FD(context, "sGameFlags"), cellForm->cellGameFlags, popupCounter, context);
            DrawUIntLine(FD(context, "sCellState"), cellForm->cellState.underlying(), popupCounter, context);
            DrawIntLine(FD(context, "sX"), mutableCell->GetDataX(), popupCounter, context);
            DrawIntLine(FD(context, "sY"), mutableCell->GetDataY(), popupCounter, context);
            DrawFormReferenceLine(FD(context, "sEncounterZone"), cellForm->GetEncounterZone(), context, popupCounter);
            DrawFormReferenceLine(FD(context, "sLocation"), cellForm->GetLocation(), context, popupCounter);
            DrawFormReferenceLine(FD(context, "sOwner"), mutableCell->GetOwner(), context, popupCounter);
            DrawFormReferenceLine(FD(context, "sWaterType"), cellForm->GetWaterType(), context, popupCounter);
            DrawFloatLine(FD(context, "sWaterHeight"), cellForm->waterHeight, popupCounter, context);
            DrawFormReferenceLine(FD(context, "sLightingTemplate"), cellForm->lightingTemplate, context, popupCounter);
            DrawUIntLine(FD(context, "sReferenceEntries"), static_cast<std::uint32_t>(cellForm->references.size()), popupCounter, context);
            if (cellForm->IsExterior()) {
                DrawFormReferenceLine(FD(context, "sWorldSpace"), cellForm->worldSpace, context, popupCounter);
            }
        }

        void DrawKeywordDetails(const RE::TESForm* form, const FormDetailsViewContext& context)
        {
            const auto keywordForm = form ? form->As<RE::BGSKeywordForm>() : nullptr;
            if (!keywordForm) {
                return;
            }

            ImGui::Separator();
            ImGui::TextUnformatted(L(context, "General", "sKeywords"));

            int displayed = 0;
            keywordForm->ForEachKeyword([&displayed, &context](RE::BGSKeyword* keyword) {
                if (!keyword || displayed >= 200) {
                    return displayed >= 200 ? RE::BSContainer::ForEachResult::kStop : RE::BSContainer::ForEachResult::kContinue;
                }

                const auto text = keyword->formEditorID.c_str();
                const auto keywordLabel = (text && text[0] != '\0') ? text : L(context, "General", "sUnnamedKeyword");
                ImGui::BulletText("%s", keywordLabel);
                const std::string keywordPopupID = "KeywordCopyPopup##" + std::to_string(displayed);
                if (ImGui::BeginPopupContextItem(keywordPopupID.c_str())) {
                    if (ImGui::MenuItem(L(context, "General", "sCopy"))) {
                        ImGui::SetClipboardText(keywordLabel);
                    }
                    ImGui::EndPopup();
                }

                ++displayed;
                return RE::BSContainer::ForEachResult::kContinue;
            });

            if (displayed == 0) {
                ImGui::TextDisabled("%s", L(context, "General", "sNoKeywords"));
            }
        }
    }

    void FormDetailsView::Draw(const FormEntry& selectedRecord, const FormDetailsViewContext& context)
    {
        int detailCopyPopupCounter = 0;

        ImGui::TextUnformatted(selectedRecord.name.empty() ? L(context, "General", "sUnnamed") : selectedRecord.name.c_str());
        DrawCopyPopup(selectedRecord.name.empty() ? L(context, "General", "sUnnamed") : selectedRecord.name, detailCopyPopupCounter, context);
        ImGui::SameLine();
        ImGui::TextDisabled("%08X", selectedRecord.formID);
        {
            char selectedFormIDBuffer[16]{};
            std::snprintf(selectedFormIDBuffer, sizeof(selectedFormIDBuffer), "%08X", selectedRecord.formID);
            DrawCopyPopup(selectedFormIDBuffer, detailCopyPopupCounter, context);
        }

        ImGui::Separator();

        DrawTextLine(L(context, "General", "sPlugin"), BuildPluginDisplayName(selectedRecord.sourcePlugin), detailCopyPopupCounter, context);
        DrawTextLine(L(context, "General", "sCategory"), selectedRecord.category, detailCopyPopupCounter, context);
        DrawTextLine(L(context, "General", "sPlayable"), selectedRecord.isPlayable ? L(context, "General", "sYes") : L(context, "General", "sNo"), detailCopyPopupCounter, context);
        DrawTextLine(L(context, "General", "sDeleted"), selectedRecord.isDeleted ? L(context, "General", "sYes") : L(context, "General", "sNo"), detailCopyPopupCounter, context);

        auto* form = RE::TESForm::GetFormByID(selectedRecord.formID);
        if (!form) {
            return;
        }

        const auto* editorID = form->GetFormEditorID();
        DrawTextLine(L(context, "General", "sEditorID"), (editorID && editorID[0] != '\0') ? editorID : L(context, "General", "sNone"), detailCopyPopupCounter, context);
        DrawTextLine(L(context, "General", "sType"), form->GetFormTypeString(), detailCopyPopupCounter, context);
        DrawUIntLine(L(context, "General", "sReferenceCount"), DataManager::GetPlacedReferenceCount(selectedRecord.formID), detailCopyPopupCounter, context);

        if (const auto* valueForm = form->As<RE::TESValueForm>()) {
            DrawIntLine(L(context, "General", "sValue"), valueForm->GetFormValue(), detailCopyPopupCounter, context);
        }

        if (const auto* weightForm = form->As<RE::TESWeightForm>()) {
            DrawFloatLine(L(context, "General", "sWeight"), weightForm->GetFormWeight(), detailCopyPopupCounter, context);
        }

        if (const auto* weaponForm = form->As<RE::TESObjectWEAP>()) {
            DrawUIntLine(L(context, "General", "sDamage"), weaponForm->weaponData.attackDamage, detailCopyPopupCounter, context);
            if (weaponForm->weaponData.attackSeconds > 0.0f) {
                DrawFloatLine(L(context, "General", "sFireRate"), 1.0f / weaponForm->weaponData.attackSeconds, detailCopyPopupCounter, context);
            }
            DrawFormReferenceLine(L(context, "Items", "sAmmo"), weaponForm->weaponData.ammo, context, detailCopyPopupCounter);
            DrawUIntLine(FD(context, "sAmmoCapacity"), weaponForm->weaponData.ammoCapacity, detailCopyPopupCounter, context);
            DrawUIntLine(FD(context, "sAttachmentParentCount"), weaponForm->attachParents.size, detailCopyPopupCounter, context);
        }

        if (const auto* ammoForm = form->As<RE::TESAmmo>()) {
            DrawFloatLine(L(context, "General", "sDamage"), ammoForm->data.damage, detailCopyPopupCounter, context);
        }

        if (const auto* armorForm = form->As<RE::TESObjectARMO>()) {
            DrawUIntLine(L(context, "General", "sArmorRating"), armorForm->armorData.rating, detailCopyPopupCounter, context);
            DrawUIntLine(FD(context, "sAttachmentParentCount"), armorForm->attachParents.size, detailCopyPopupCounter, context);
        }

        if (const auto* npcForm = form->As<RE::TESNPC>()) {
            DrawIntLine(L(context, "General", "sLevel"), npcForm->GetLevel(), detailCopyPopupCounter, context);
            DrawFormReferenceLine(L(context, "General", "sRace"), npcForm->GetFormRace(), context, detailCopyPopupCounter);
            DrawUIntLine(FD(context, "sAttachmentParentCount"), npcForm->attachParents.size, detailCopyPopupCounter, context);
        }

        if (const auto* soundForm = form->As<RE::TESSound>()) {
            DrawFormReferenceLine(L(context, "General", "sDescriptor"), soundForm->descriptor, context, detailCopyPopupCounter);
        }

        if (context.showAdvancedDetailsView) {
            if (const auto* weaponForm = form->As<RE::TESObjectWEAP>()) {
                DrawAdvancedWeaponDetails(weaponForm, context, detailCopyPopupCounter);
            }
            if (const auto* armorForm = form->As<RE::TESObjectARMO>()) {
                DrawAdvancedArmorDetails(armorForm, context, detailCopyPopupCounter);
            }
            if (const auto* npcForm = form->As<RE::TESNPC>()) {
                DrawAdvancedNPCDetails(npcForm, context, detailCopyPopupCounter);
            }
            if (const auto* soundForm = form->As<RE::TESSound>()) {
                DrawAdvancedSoundDetails(soundForm, context, detailCopyPopupCounter);
            }
            if (const auto* globalForm = form->As<RE::TESGlobal>()) {
                DrawAdvancedGlobalDetails(globalForm, context, detailCopyPopupCounter);
            }
            if (const auto* outfitForm = form->As<RE::BGSOutfit>()) {
                DrawAdvancedOutfitDetails(outfitForm, context, detailCopyPopupCounter);
            }
            if (const auto* weatherForm = form->As<RE::TESWeather>()) {
                DrawAdvancedWeatherDetails(weatherForm, context, detailCopyPopupCounter);
            }
            if (const auto* activatorForm = form->As<RE::TESObjectACTI>()) {
                DrawAdvancedActivatorDetails(activatorForm, context, detailCopyPopupCounter);
            }
            if (const auto* containerForm = form->As<RE::TESObjectCONT>()) {
                DrawAdvancedContainerDetails(containerForm, context, detailCopyPopupCounter);
            }
            if (const auto* staticForm = form->As<RE::TESObjectSTAT>()) {
                DrawAdvancedStaticDetails(staticForm, context, detailCopyPopupCounter);
            }
            if (const auto* furnitureForm = form->As<RE::TESFurniture>()) {
                DrawAdvancedFurnitureDetails(furnitureForm, context, detailCopyPopupCounter);
            }
            if (const auto* spellForm = form->As<RE::SpellItem>()) {
                DrawAdvancedSpellDetails(spellForm, context, detailCopyPopupCounter);
            }
            if (const auto* perkForm = form->As<RE::BGSPerk>()) {
                DrawAdvancedPerkDetails(perkForm, context, detailCopyPopupCounter);
            }
            if (const auto* ammoForm = form->As<RE::TESAmmo>()) {
                DrawAdvancedAmmoDetails(ammoForm, context, detailCopyPopupCounter);
            }
            if (const auto* alchemyForm = form->As<RE::AlchemyItem>()) {
                DrawAdvancedAlchemyDetails(alchemyForm, context, detailCopyPopupCounter);
            }
            if (const auto* constructibleForm = form->As<RE::BGSConstructibleObject>()) {
                DrawAdvancedConstructibleDetails(constructibleForm, context, detailCopyPopupCounter);
            }
            if (const auto* questForm = form->As<RE::TESQuest>()) {
                DrawAdvancedQuestDetails(questForm, context, detailCopyPopupCounter);
            }
            if (const auto* cellForm = form->As<RE::TESObjectCELL>()) {
                DrawAdvancedCellDetails(cellForm, context, detailCopyPopupCounter);
            }
        }

        DrawKeywordDetails(form, context);
    }
}
