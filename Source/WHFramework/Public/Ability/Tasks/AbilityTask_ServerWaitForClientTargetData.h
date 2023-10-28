// Copyright 2020 Dan Kestranek.

#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilityTask_ServerWaitForClientTargetData.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityTask_ServerWaitForClientTargetData : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitTargetDataDelegate	ValidData;

	UFUNCTION(BlueprintCallable, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true", HideSpawnParms = "Instigator"), Category = "Ability|Tasks")
	static UAbilityTask_ServerWaitForClientTargetData* ServerWaitForClientTargetData(UGameplayAbility* OwningAbility, FName TaskInstanceName, bool TriggerOnce);

	virtual void Activate() override;

	UFUNCTION()
	void OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag Activation);

protected:
	virtual void OnDestroy(bool AbilityEnded) override;

	bool bTriggerOnce;
};
