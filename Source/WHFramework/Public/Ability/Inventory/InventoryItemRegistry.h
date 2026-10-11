#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "Ability/Inventory/InventoryTypes.h"
#include "InventoryItemRegistry.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventoryPropertyRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Key;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SchemaVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EInventoryPropertyType Type = EInventoryPropertyType::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 Minimum = MIN_int64;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 Maximum = MAX_int64;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MaximumNameLength = 64;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bRequired = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FName> AllowedNames;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventoryItemDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FPrimaryAssetId ItemId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FSoftObjectPath Icon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTagContainer Tags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MaxStack = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MaxLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EInventoryStackPolicy StackPolicy = EInventoryStackPolicy::Stackable;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EInventoryUsePolicy UsePolicy = EInventoryUsePolicy::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag EquipPart;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EInventoryHandPolicy HandPolicy = EInventoryHandPolicy::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag WeaponType;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag ToolType;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FSoftClassPath AbilityClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FSoftClassPath> PassiveEffects;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FSoftClassPath EquipmentActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FPrimaryAssetId PlaceableVoxelId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ConsumableCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bPassiveSkill = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bCancelAbilityOnRemove = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bCanTrade = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bCanDrop = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FInventoryPropertyRule> PropertyRules;
};

class WHFRAMEWORK_API FInventoryItemRegistry
{
public:
    bool Initialize(TConstArrayView<FInventoryItemDefinition> InDefinitions, FString& OutError);
    bool IsInitialized() const;
    const FInventoryItemDefinition* Find(const FPrimaryAssetId& InItemId) const;
    bool ValidateItem(const FAbilityItem& InItem, FString& OutError) const;
    bool ValidateDefinition(const FInventoryItemDefinition& InDefinition, FString& OutError) const;
    void GetDefinitions(TArray<FInventoryItemDefinition>& OutDefinitions) const;

private:
    TMap<FPrimaryAssetId, FInventoryItemDefinition> Definitions;
    bool bInitialized = false;
};
