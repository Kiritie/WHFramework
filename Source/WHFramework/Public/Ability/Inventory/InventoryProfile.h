#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Ability/Inventory/InventoryTypes.h"
#include "InventoryProfile.generated.h"

class FInventoryItemRegistry;
struct FInventoryItemDefinition;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventorySlotRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTagContainer RequiredAllTags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTagContainer RequiredAnyTags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTagContainer BlockedTags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MaxStackOverride = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bAllowsInsert = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bAllowsRemove = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bAllowsAutomatic = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bAllowsSort = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bInitiallyLocked = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag EquipPart;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 WeaponGroup = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FInventorySlotKey OppositeHandSlot;

    int32 GetCapacity(const FInventoryItemDefinition& InDefinition) const;
    bool Accepts(const FInventoryItemDefinition& InDefinition) const;
    bool operator==(const FInventorySlotRule& InOther) const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventorySlotRuleOverride
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Index = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FInventorySlotRule Rule;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventorySlotGroupDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag Group;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Count = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Order = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 AddPriority = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 RemovePriority = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FInventorySlotRule DefaultRule;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FInventorySlotRuleOverride> Overrides;
};

struct WHFRAMEWORK_API FInventorySlotDefinition
{
    FInventorySlotKey Key;
    FInventorySlotRule Rule;
    int32 Order = 0;
    int32 AddPriority = 0;
    int32 RemovePriority = 0;
};

struct WHFRAMEWORK_API FInventoryProfileDefinition
{
    FPrimaryAssetId ProfileId;
    TArray<FInventorySlotDefinition> Slots;

    static bool Build(const FPrimaryAssetId& InProfileId, TConstArrayView<FInventorySlotGroupDefinition> InGroups, FInventoryProfileDefinition& OutDefinition, FString& OutError);
    bool ValidateDefinition(FString& OutError) const;
    bool ValidateState(const FInventorySnapshot& InState, const FInventoryItemRegistry& InRegistry, FString& OutError) const;
    const FInventorySlotDefinition* FindSlot(const FInventorySlotKey& InKey) const;
    FInventorySnapshot CreateState(const FGuid& InInventoryId, const FGuid& InWorldId, const FGuid& InOwnerId, uint32 InGeneration = 1) const;
};

UCLASS(BlueprintType)
class WHFRAMEWORK_API UInventoryProfile : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FInventorySlotGroupDefinition> Groups;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
    bool Compile(FInventoryProfileDefinition& OutDefinition, FString& OutError) const;
};
