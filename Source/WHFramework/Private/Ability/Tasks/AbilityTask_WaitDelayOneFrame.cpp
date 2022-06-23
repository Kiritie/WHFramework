// Copyright 2020 Dan Kestranek.


#include "Ability/Tasks/AbilityTask_WaitDelayOneFrame.h"
#include "TimerManager.h"

UAbilityTask_WaitDelayOneFrame::UAbilityTask_WaitDelayOneFrame(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAbilityTask_WaitDelayOneFrame::Activate()
{
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UAbilityTask_WaitDelayOneFrame::OnDelayFinish);
}

UAbilityTask_WaitDelayOneFrame* UAbilityTask_WaitDelayOneFrame::WaitDelayOneFrame(UGameplayAbility* OwningAbility)
{
	UAbilityTask_WaitDelayOneFrame* MyObj = NewAbilityTask<UAbilityTask_WaitDelayOneFrame>(OwningAbility);
	return MyObj;
}

void UAbilityTask_WaitDelayOneFrame::OnDelayFinish()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinish.Broadcast();
	}
	EndTask();
}
