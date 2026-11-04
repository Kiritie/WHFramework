#pragma once

#include "Ability/Abilities/AbilityBase.h"

#include "EffectAbilityBase.generated.h"

class UEffectBase;

/** 提供可配置效果及其动作生命周期的公共能力基类。 */
UCLASS()
class WHFRAMEWORK_API UEffectAbilityBase : public UAbilityBase
{
	GENERATED_BODY()

public:
	UEffectAbilityBase();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameplayEffects")
	TArray<TSubclassOf<UEffectBase>> EffectClasses;

	virtual TArray<TSubclassOf<UEffectBase>> GetEffectClasses() const override;

protected:
	virtual void BeginAction_Implementation() override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UPROPERTY(Transient)
	TArray<FActiveGameplayEffectHandle> EffectHandles;

	bool bCleaningEffects = false;
};
