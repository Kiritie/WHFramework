#pragma once

#include "AbilityBase.h"

#include "VitalityActionAbilityBase.generated.h"

/**
 * 生命体动作能力
 */
UCLASS()
class WHFRAMEWORK_API UVitalityActionAbilityBase : public UAbilityBase
{
	GENERATED_BODY()

public:
	UVitalityActionAbilityBase();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

};
