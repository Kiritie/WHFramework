#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Ability/Inventory/InventoryItemRegistry.h"
#include "Asset/Primary/PrimaryAssetBase.h"
#include "GameplayEffectTypes.h"

#include "AbilityItemDataBase.generated.h"

class UItemAbilityBase;
class AInventoryPickupActor;
class UEffectBase;

struct WHFRAMEWORK_API FAbilityItemAttributeValue
{
    FGameplayAttribute Attribute;
    TEnumAsByte<EGameplayModOp::Type> ModifierOp = EGameplayModOp::Additive;
    EGameplayModEvaluationChannel Channel = EGameplayModEvaluationChannel::Channel0;
    float Value = 0.f;
    bool bConditional = false;
};

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityItemDataBase : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UAbilityItemDataBase();

public:
	virtual void OnInitialize_Implementation() override;
	
	virtual void OnReset_Implementation() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true"))
	FText Detail;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAbilityItemRarity Rarity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowedClasses = "/Script/Engine.Texture,/Script/Engine.MaterialInterface"))
	UObject* Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxLevel;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UItemAbilityBase> AbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AInventoryPickupActor> PickUpClass;

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EInventoryStackPolicy StackPolicy = EInventoryStackPolicy::Stackable;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EInventoryUsePolicy UsePolicy = EInventoryUsePolicy::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTagContainer InventoryTags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FInventoryPropertyRule> PropertyRules;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<TSubclassOf<UGameplayEffect>> PassiveEffects;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ConsumableCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bCanTrade = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bCanDrop = true;

protected:
	UPROPERTY(Transient)
	UObject* _Icon;

public:
	UFUNCTION(BlueprintPure)
	int32 ClampCount(int32 InCount) const;

	UFUNCTION(BlueprintPure)
	int32 ClampLevel(int32 InLevel) const;

	UFUNCTION(BlueprintPure)
	EAbilityItemType GetItemType() const;

	UFUNCTION(BlueprintPure)
	virtual FString GetItemAbilityInfo(int32 InLevel = 1) const;

	UFUNCTION(BlueprintPure)
	virtual FString GetItemErrorInfo(FAbilityItem InItem, int32 InActorLevel) const;

	virtual FInventoryItemDefinition BuildInventoryDefinition() const;

    static bool GetEquipmentAttributeValues(const FInventoryItemDefinition& InDefinition, int32 InLevel, TArray<FAbilityItemAttributeValue>& OutValues);
    static bool GetEffectAttributeValues(const TArray<TSubclassOf<UEffectBase>>& InEffects, int32 InLevel, TArray<FAbilityItemAttributeValue>& OutValues);
    static FText FormatAttributeName(const FAbilityItemAttributeValue& InValue);
    static FText FormatAttributeValue(const FAbilityItemAttributeValue& InValue, float InMagnitude, bool bInSigned);
};

