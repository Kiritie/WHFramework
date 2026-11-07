#pragma once

#include "Ability/Inventory/InventoryItemTypes.h"
#include "Ability/Abilities/EffectAbilityBase.h"

#include "ItemAbilityBase.generated.h"

/**
 * 物品Ability基类
 */
UCLASS()
class WHFRAMEWORK_API UItemAbilityBase : public UEffectAbilityBase
{
	GENERATED_BODY()

public:
	UItemAbilityBase();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	UFUNCTION(BlueprintPure, Category="Inventory")
	bool GetInventoryItem(FAbilityItem& OutItem) const;
};
