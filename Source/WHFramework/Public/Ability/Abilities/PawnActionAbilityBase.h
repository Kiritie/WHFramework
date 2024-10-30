#pragma once

#include "AbilityBase.h"

#include "PawnActionAbilityBase.generated.h"

/**
 * ��ɫ��ΪAbility����
 */
UCLASS()
class WHFRAMEWORK_API UPawnActionAbilityBase : public UAbilityBase
{
	GENERATED_BODY()

public:
	UPawnActionAbilityBase();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	//UPROPERTY(BlueprintReadWrite)
	bool bWasStopped;

public:
	bool WasStopped() const { return bWasStopped; }

	void SetStopped(bool val) { bWasStopped = val; }
};
