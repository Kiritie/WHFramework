// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Tasks/AbilityTask_WaitForEvent.h"

#include "GameFramework/Character.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Animation/AnimInstance.h"

UAbilityTask_WaitForEvent::UAbilityTask_WaitForEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAbilitySystemComponentBase* UAbilityTask_WaitForEvent::GetTargetASC()
{
	return Cast<UAbilitySystemComponentBase>(AbilitySystemComponent.Get());
}

void UAbilityTask_WaitForEvent::OnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		FGameplayEventData TempData = *Payload;
		TempData.EventTag = EventTag;

		EventReceived.Broadcast(EventTag, TempData);
	}
}

UAbilityTask_WaitForEvent* UAbilityTask_WaitForEvent::WaitForEvent(UGameplayAbility* OwningAbility,
	FName TaskInstanceName, FGameplayTagContainer EventTags)
{
	UAbilityTask_WaitForEvent* MyObj = NewAbilityTask<UAbilityTask_WaitForEvent>(OwningAbility, TaskInstanceName);
	MyObj->EventTags = EventTags;

	return MyObj;
}

void UAbilityTask_WaitForEvent::Activate()
{
	if (Ability == nullptr)
	{
		return;
	}

	UAbilitySystemComponentBase* AbilitySystemComponentBase = GetTargetASC();

	if (AbilitySystemComponentBase)
	{
		EventHandle = AbilitySystemComponentBase->AddGameplayEventTagContainerDelegate(EventTags, FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &UAbilityTask_WaitForEvent::OnGameplayEvent));
	}
	else
	{
		WHLog(WH_Ability, Warning, TEXT("UAbilityTask_WaitForEvent called on invalid AbilitySystemComponent"));
	}

	SetWaitingOnAvatar();
}

void UAbilityTask_WaitForEvent::ExternalCancel()
{
	check(AbilitySystemComponent.Get());

	Super::ExternalCancel();
}

void UAbilityTask_WaitForEvent::OnDestroy(bool AbilityEnded)
{
	// Note: Clearing montage end delegate isn't necessary since its not a multicast and will be cleared when the next montage plays.
	// (If we are destroyed, it will detect this and not do anything)

	UAbilitySystemComponentBase* AbilitySystemComponentBase = GetTargetASC();
	if (AbilitySystemComponentBase)
	{
		AbilitySystemComponentBase->RemoveGameplayEventTagContainerDelegate(EventTags, EventHandle);
	}

	Super::OnDestroy(AbilityEnded);

}
