// Copyright 2020 Dan Kestranek.
#include "Ability/Tasks/AbilityTask_ServerWaitForClientTargetData.h"

#include "AbilitySystemComponent.h"
#include "Ability/Abilities/AbilityBase.h"
#include "TimerManager.h"
#include "Engine/World.h"

UAbilityTask_ServerWaitForClientTargetData::UAbilityTask_ServerWaitForClientTargetData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAbilityTask_ServerWaitForClientTargetData* UAbilityTask_ServerWaitForClientTargetData::ServerWaitForClientTargetData(UGameplayAbility* OwningAbility, FName TaskInstanceName, bool TriggerOnce, float TimeoutSeconds)
{
	UAbilityTask_ServerWaitForClientTargetData* Task = NewAbilityTask<UAbilityTask_ServerWaitForClientTargetData>(OwningAbility, TaskInstanceName);
	Task->bTriggerOnce = TriggerOnce;
	Task->WaitSeconds = FMath::IsFinite(TimeoutSeconds) ? FMath::Clamp(TimeoutSeconds, 0.1f, 30.f) : 2.f;
	return Task;
}

void UAbilityTask_ServerWaitForClientTargetData::Activate()
{
	if (!Ability || !AbilitySystemComponent.IsValid() || !Ability->GetCurrentActorInfo() || !Ability->GetCurrentActorInfo()->IsNetAuthority())
	{
		Reject(AbilityActionTags::NotReady);
		return;
	}
	const FGameplayAbilitySpecHandle Handle = GetAbilitySpecHandle();
	const FPredictionKey Key = GetActivationPredictionKey();
	DataDelegate = AbilitySystemComponent->AbilityTargetDataSetDelegate(Handle, Key).AddUObject(this, &ThisClass::OnTargetDataReplicatedCallback);
	CancelDelegate = AbilitySystemComponent->AbilityTargetDataCancelledDelegate(Handle, Key).AddUObject(this, &ThisClass::OnTargetDataCancelled);
	SetWaitingOnRemotePlayerData();
	ArmTimeout();
	AbilitySystemComponent->CallReplicatedTargetDataDelegatesIfSet(Handle, Key);
}

void UAbilityTask_ServerWaitForClientTargetData::OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag Activation)
{
	if (bFinished || !AbilitySystemComponent.IsValid())
	{
		return;
	}
	const FGameplayAbilityTargetDataHandle Received = Data;
	AbilitySystemComponent->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());
	UAbilityBase* Action = Cast<UAbilityBase>(Ability);
	FGameplayTag Reason;
	if (!Action || !Action->ValidateReplicatedTargetData(Received, Reason))
	{
		Reject(Reason.IsValid() ? Reason : AbilityActionTags::InvalidTarget);
		return;
	}
	bFinished = bTriggerOnce;
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		ValidData.Broadcast(Received);
	}
	if (bFinished)
	{
		EndTask();
	}
	else
	{
		ArmTimeout();
	}
}

void UAbilityTask_ServerWaitForClientTargetData::OnTargetDataCancelled()
{
	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());
	}
	Reject(AbilityActionTags::Cancelled);
}

void UAbilityTask_ServerWaitForClientTargetData::OnTimeout()
{
	Reject(AbilityActionTags::TargetDataTimeout);
}

void UAbilityTask_ServerWaitForClientTargetData::Reject(FGameplayTag Reason)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	TWeakObjectPtr<UAbilityBase> Action = Cast<UAbilityBase>(Ability);
	const FGuid RejectedId = Action.IsValid() ? Action->GetActionActivationId() : FGuid();
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		Cancelled.Broadcast(FGameplayAbilityTargetDataHandle());
	}
	EndTask();
	if (Action.IsValid() && Action->IsActive() && Action->GetActionActivationId() == RejectedId)
	{
		Action->FailAction(Reason);
	}
}

void UAbilityTask_ServerWaitForClientTargetData::ArmTimeout()
{
	if (!bFinished && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(TimeoutTimer, this, &ThisClass::OnTimeout, WaitSeconds, false);
	}
}

void UAbilityTask_ServerWaitForClientTargetData::OnDestroy(bool AbilityEnded)
{
	bFinished = true;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimeoutTimer);
	}
	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->AbilityTargetDataSetDelegate(GetAbilitySpecHandle(), GetActivationPredictionKey()).Remove(DataDelegate);
		AbilitySystemComponent->AbilityTargetDataCancelledDelegate(GetAbilitySpecHandle(), GetActivationPredictionKey()).Remove(CancelDelegate);
	}
	Super::OnDestroy(AbilityEnded);
}
