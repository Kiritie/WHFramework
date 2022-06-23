// Copyright 2020 Dan Kestranek.


#include "Ability/Tasks/AbilityTask_WaitInputPressWithTags.h"
#include "AbilitySystemComponent.h"

UAbilityTask_WaitInputPressWithTags::UAbilityTask_WaitInputPressWithTags(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StartTime = 0.f;
	bTestInitialState = false;
}

UAbilityTask_WaitInputPressWithTags* UAbilityTask_WaitInputPressWithTags::WaitInputPressWithTags(UGameplayAbility* OwningAbility, FGameplayTagContainer RequiredTags, FGameplayTagContainer IgnoredTags, bool bTestAlreadyPressed)
{
	UAbilityTask_WaitInputPressWithTags* Task = NewAbilityTask<UAbilityTask_WaitInputPressWithTags>(OwningAbility);
	Task->bTestInitialState = bTestAlreadyPressed;
	Task->RequiredTags = RequiredTags;
	Task->IgnoredTags = IgnoredTags;
	return Task;
}

void UAbilityTask_WaitInputPressWithTags::Activate()
{
	StartTime = GetWorld()->GetTimeSeconds();
	if (Ability)
	{
		if (bTestInitialState && IsLocallyControlled())
		{
			FGameplayAbilitySpec* Spec = Ability->GetCurrentAbilitySpec();
			if (Spec && Spec->InputPressed)
			{
				OnPressCallback();
				return;
			}
		}

		DelegateHandle = AbilitySystemComponent->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputPressed, GetAbilitySpecHandle(), GetActivationPredictionKey()).AddUObject(this, &UAbilityTask_WaitInputPressWithTags::OnPressCallback);
		if (IsForRemoteClient())
		{
			if (!AbilitySystemComponent->CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::InputPressed, GetAbilitySpecHandle(), GetActivationPredictionKey()))
			{
				SetWaitingOnRemotePlayerData();
			}
		}
	}
}

void UAbilityTask_WaitInputPressWithTags::OnPressCallback()
{
	float ElapsedTime = GetWorld()->GetTimeSeconds() - StartTime;

	if (!Ability || !AbilitySystemComponent)
	{
		EndTask();
		return;
	}

	//TODO move this into a tag query
	if (AbilitySystemComponent->HasAnyMatchingGameplayTags(IgnoredTags) || !AbilitySystemComponent->HasAllMatchingGameplayTags(RequiredTags))
	{
		Reset();
		return;
	}

	//TODO extend tag query to support this and move this into it
	// Hardcoded for GA_InteractPassive to ignore input while already interacting
	if (AbilitySystemComponent->GetTagCount(FGameplayTag::RequestGameplayTag("State.Interacting"))
		> AbilitySystemComponent->GetTagCount(FGameplayTag::RequestGameplayTag("State.InteractingRemoval")))
	{
		Reset();
		return;
	}

	AbilitySystemComponent->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputPressed, GetAbilitySpecHandle(), GetActivationPredictionKey()).Remove(DelegateHandle);

	FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent, IsPredictingClient());

	if (IsPredictingClient())
	{
		// Tell the server about this
		AbilitySystemComponent->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, GetAbilitySpecHandle(), GetActivationPredictionKey(), AbilitySystemComponent->ScopedPredictionKey);
	}
	else
	{
		AbilitySystemComponent->ConsumeGenericReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, GetAbilitySpecHandle(), GetActivationPredictionKey());
	}

	// We are done. Kill us so we don't keep getting broadcast messages
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnPress.Broadcast(ElapsedTime);
	}

	EndTask();
}

void UAbilityTask_WaitInputPressWithTags::OnDestroy(bool AbilityEnded)
{
	AbilitySystemComponent->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputPressed, GetAbilitySpecHandle(), GetActivationPredictionKey()).Remove(DelegateHandle);

	ClearWaitingOnRemotePlayerData();

	Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_WaitInputPressWithTags::Reset()
{
	AbilitySystemComponent->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputPressed, GetAbilitySpecHandle(), GetActivationPredictionKey()).Remove(DelegateHandle);

	DelegateHandle = AbilitySystemComponent->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputPressed, GetAbilitySpecHandle(), GetActivationPredictionKey()).AddUObject(this, &UAbilityTask_WaitInputPressWithTags::OnPressCallback);
	if (IsForRemoteClient())
	{
		if (!AbilitySystemComponent->CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::InputPressed, GetAbilitySpecHandle(), GetActivationPredictionKey()))
		{
			SetWaitingOnRemotePlayerData();
		}
	}
}
