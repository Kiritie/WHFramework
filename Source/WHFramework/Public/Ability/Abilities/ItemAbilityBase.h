#pragma once

#include "Ability/Abilities/AbilityBase.h"

#include "ItemAbilityBase.generated.h"

/**
 * 物品Ability基类
 */
UCLASS()
class WHFRAMEWORK_API UItemAbilityBase : public UAbilityBase
{
	GENERATED_BODY()

public:
	UItemAbilityBase();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffects")
	TArray<TSubclassOf<UGameplayEffect>> ItemEffectClasses;

private:
	TArray<FActiveGameplayEffectHandle> ItemEffectHandles;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
};
