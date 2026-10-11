// Copyright 2020 Dan Kestranek.
#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilityTask_ServerWaitForClientTargetData.generated.h"

UCLASS()
class WHFRAMEWORK_API UAbilityTask_ServerWaitForClientTargetData : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAbilityTask_ServerWaitForClientTargetData(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable)
	FWaitTargetDataDelegate ValidData;

	UPROPERTY(BlueprintAssignable)
	FWaitTargetDataDelegate Cancelled;

	UFUNCTION(BlueprintCallable, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true", HideSpawnParms = "Instigator"), Category = "Ability|Tasks")
	static UAbilityTask_ServerWaitForClientTargetData* ServerWaitForClientTargetData(UGameplayAbility* OwningAbility, FName TaskInstanceName, bool TriggerOnce, float TimeoutSeconds = 2.f);

	virtual void Activate() override;

	UFUNCTION()
	void OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag Activation);

protected:
	virtual void OnDestroy(bool AbilityEnded) override;

private:
	void OnTargetDataCancelled();
	void OnTimeout();
	void Reject(FGameplayTag Reason);
	void ArmTimeout();

	bool bTriggerOnce = true;
	bool bFinished = false;
	float WaitSeconds = 2.f;
	FDelegateHandle DataDelegate;
	FDelegateHandle CancelDelegate;
	FTimerHandle TimeoutTimer;
};
