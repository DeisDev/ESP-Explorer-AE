#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ESPExplorerAE
{
    struct DetailKey
    {
        std::uint32_t formID{};
        std::uint64_t session{};
        std::uint64_t catalogGeneration{};
        bool advanced{};
        bool operator==(const DetailKey&) const = default;
    };
    struct DetailReference
    {
        std::uint32_t formID{};
        std::string name;
        std::string editorID;
    };
    struct DetailItem { DetailReference record; std::int64_t quantity{}; };
    struct DetailAlias { std::uint32_t id{}; std::string name; std::uint32_t flags{}; };
    struct DetailObjective { std::uint32_t index{}; std::string text; std::uint32_t state{}; std::vector<std::uint32_t> aliases; };
    struct DetailFormValue { DetailReference form; float value{}; };

    struct WeaponDetails
    {
        std::string model{};
        DetailReference equipmentType{};
        DetailReference blockBashImpactData{};
        DetailReference blockBashMaterial{};
        DetailReference instanceNamingRules{};
        DetailReference impactDataSet{};
        DetailReference onHitEffect{};
        DetailReference ammo{};
        DetailReference skill{};
        DetailReference resist{};
        float speed{};
        float reloadSpeed{};
        float reach{};
        float minRange{};
        float maxRange{};
        float attackDelay{};
        float damageOutOfRangeMult{};
        float damageOnHitMult{};
        float damageSecondary{};
        float weight{};
        std::uint32_t value{};
        std::uint32_t damageBase{};
        float actionPointCost{};
        float criticalChargeBonus{};
        float criticalDamageMult{};
        float soundLevelMult{};
        float fireSeconds{};
        float reloadSeconds{};
        std::uint32_t projectiles{};
        DetailReference overrideProjectile{};
        std::uint32_t ammoCapacity{};
        std::uint32_t attachmentParentCount{};
        std::optional<float> fireRate{};
        bool hasRangedData{};
        bool hasDamageTypes{};
        bool hasActorValues{};
        std::vector<DetailFormValue> damageTypes{};
        std::vector<DetailFormValue> actorValues{};
    };

    struct ArmorDetails
    {
        DetailReference equipmentType{};
        DetailReference blockBashImpactData{};
        DetailReference blockBashMaterial{};
        DetailReference instanceNamingRules{};
        std::uint32_t armorRating{};
        std::uint32_t health{};
        std::uint32_t value{};
        float weight{};
        float colorRemapIndex{};
        std::string model{};
        std::uint32_t addonCount{};
        std::uint32_t damageTypeEntries{};
        std::uint32_t actorValueEntries{};
        std::uint32_t attachmentParentCount{};
        bool hasDamageTypes{};
        bool hasActorValues{};
    };

    struct NPCDetails
    {
        float height{};
        float heightMax{};
        DetailReference npcClass{};
        DetailReference combatStyle{};
        DetailReference defaultOutfit{};
        DetailReference sleepOutfit{};
        DetailReference crimeFaction{};
        int level{};
        DetailReference resolvedRace{};
        std::uint32_t attachmentParentCount{};
    };

    struct SoundDetails
    {
        DetailReference descriptor{};
        float minDelay{};
        float maxDelay{};
        bool stackable{};
    };

    struct AmmoDetails
    {
        std::string model{};
        std::string shellCasingModel{};
        std::uint32_t health{};
        std::uint32_t flags{};
        DetailReference projectile{};
        float damage{};
    };

    struct AlchemyDetails
    {
        std::string model{};
        DetailReference equipmentType{};
        DetailReference addictionItem{};
        float addictionChance{};
        DetailReference consumptionSound{};
    };

    struct GlobalDetails
    {
        float value{};
    };

    struct OutfitDetails
    {
        std::uint32_t outfitItems{};
        std::vector<DetailItem> items;
        bool truncated{};
    };

    struct WeatherDetails
    {
        std::uint32_t flags{};
        std::uint32_t cloudLayers{};
        std::uint32_t skyStatics{};
        float volatilityMult{};
        float visibilityMult{};
        std::string auroraModel{};
    };

    struct ActivatorDetails
    {
        std::string model{};
        DetailReference materialSwap{};
        float colorRemapIndex{};
        DetailReference loopSound{};
        DetailReference activateSound{};
        DetailReference waterType{};
        std::uint32_t flags{};
    };

    struct ContainerDetails
    {
        std::string model{};
        DetailReference materialSwap{};
        float colorRemapIndex{};
        float weight{};
        std::uint32_t flags{};
        std::uint32_t containerEntries{};
        std::uint32_t containerItems{};
        DetailReference openSound{};
        DetailReference closeSound{};
        DetailReference takeAllSound{};
        DetailReference containsOnlyList{};
        std::vector<DetailItem> contents;
        bool truncated{};
    };

    struct StaticDetails
    {
        std::string model{};
        DetailReference materialSwap{};
        float colorRemapIndex{};
        float materialThresholdAngle{};
        float leafAmplitude{};
        float leafFrequency{};
    };

    struct FurnitureDetails
    {
        std::uint32_t entryPoints{};
        std::uint32_t markerCount{};
        std::uint32_t attachmentParentCount{};
        std::uint32_t flags{};
        int workbenchType{};
        DetailReference associatedForm{};
        std::uint32_t containerEntries{};
        std::uint32_t containerItems{};
    };

    struct SpellDetails
    {
        DetailReference equipmentType{};
        int costOverride{};
        std::uint32_t flags{};
        float castDuration{};
        float range{};
        float chargeTime{};
        DetailReference castingPerk{};
        std::uint32_t effectEntries{};
        int hostileCount{};
        std::uint32_t preloadCount{};
    };

    struct PerkDetails
    {
        bool trait{};
        bool playable{};
        bool hidden{};
        int level{};
        int numRanks{};
        std::uint32_t perkEntries{};
        DetailReference nextPerk{};
        DetailReference sound{};
        std::string swfFile{};
    };

    struct ConstructibleDetails
    {
        DetailReference createdItem{};
        DetailReference benchKeyword{};
        std::uint32_t requiredItems{};
        std::uint32_t constructedCount{};
        std::uint32_t workshopPriority{};
        std::vector<DetailItem> requirements;
        bool truncated{};
    };

    struct QuestDetails
    {
        std::uint32_t currentStage{};
        std::uint32_t eventID{};
        std::uint32_t stages{};
        std::uint32_t objectives{};
        std::uint32_t aliases{};
        std::uint32_t aliasedReferences{};
        bool alreadyRun{};
        float delayTime{};
        int priority{};
        int questType{};
        std::uint32_t flags{};
        std::vector<DetailAlias> aliasList;
        std::vector<DetailObjective> objectiveList;
        bool truncated{};
    };

    struct CellDetails
    {
        bool interior{};
        bool hasWater{};
        bool cantWaitHere{};
        std::uint32_t flags{};
        std::uint32_t gameFlags{};
        std::uint32_t cellState{};
        int x{};
        int y{};
        DetailReference encounterZone{};
        DetailReference location{};
        DetailReference owner{};
        DetailReference waterType{};
        float waterHeight{};
        DetailReference lightingTemplate{};
        std::uint32_t referenceEntries{};
        DetailReference worldSpace{};
        bool exterior{};
    };

    using RecordTypeDetails = std::variant<std::monostate, WeaponDetails, ArmorDetails, NPCDetails, SoundDetails, AmmoDetails, AlchemyDetails, GlobalDetails, OutfitDetails, WeatherDetails, ActivatorDetails, ContainerDetails, StaticDetails, FurnitureDetails, SpellDetails, PerkDetails, ConstructibleDetails, QuestDetails, CellDetails>;

    enum class DetailReadStatus { Available, Missing, Failed };
    struct RecordDetails
    {
        DetailKey key;
        DetailReadStatus status{ DetailReadStatus::Missing };
        std::string editorID;
        std::string type;
        std::optional<int> value;
        std::optional<float> weight;
        std::optional<std::vector<std::string>> keywords;
        RecordTypeDetails specific;
    };
}
