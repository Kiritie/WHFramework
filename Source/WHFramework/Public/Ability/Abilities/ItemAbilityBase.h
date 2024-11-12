#pragma once

#include "Ability/Abilities/AbilityBase.h"

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

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffects")
	TArray<TSubclassOf<UEffectBase>> EffectClasses;

private:
	TArray<FActiveGameplayEffectHandle> EffectHandles;

public:
	virtual TArray<TSubclassOf<UEffectBase>> GetEffectClasses() const override;
};
