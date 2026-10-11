#pragma once

#include "Ability/Abilities/AbilityBase.h"
#include "Ability/Inventory/InventoryTypes.h"

#include "ItemAbilityBase.generated.h"

class UEffectBase;
/**
 * 物品Ability基类
 */
UCLASS()
class WHFRAMEWORK_API UItemAbilityBase : public UAbilityBase
{
	GENERATED_BODY()

public:
	UItemAbilityBase();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	UFUNCTION(BlueprintPure, Category="Inventory")
	bool GetInventoryItem(FAbilityItem& OutItem) const;

protected:
	virtual void BeginAction_Implementation() override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffects")
	TArray<TSubclassOf<UEffectBase>> EffectClasses;

private:
	UPROPERTY(Transient)
	TArray<FActiveGameplayEffectHandle> EffectHandles;
	bool bCleaningEffects = false;

public:
	virtual TArray<TSubclassOf<UEffectBase>> GetEffectClasses() const override;
};
