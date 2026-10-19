#include "Widget/Animator/WidgetAnimationAnimator.h"

#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationState.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UnrealType.h"

void UWidgetAnimationCompletion::Execute()
{
	if (UWidgetAnimationAnimator* Animator = Owner.Get())
	{
		Animator->OnAnimationCompleted(Session);
	}
}

UWidgetAnimationAnimator::UWidgetAnimationAnimator()
{
}

void UWidgetAnimationAnimator::OnSpawn_Implementation(const FParameter& InParam)
{
	Stop_Implementation(false);
	Super::OnSpawn_Implementation(InParam);
	const FObjectPropertyBase* Property = ParentWidget ? FindFProperty<FObjectPropertyBase>(ParentWidget->GetClass(), AnimationName) : nullptr;
	Animation = Property ? Cast<UWidgetAnimation>(Property->GetObjectPropertyValue_InContainer(ParentWidget)) : nullptr;
}

void UWidgetAnimationAnimator::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Stop_Implementation(false);
	Animation = nullptr;
	ParentWidget = nullptr;
	TargetWidget = nullptr;
	Super::OnDespawn_Implementation(InMode);
}

void UWidgetAnimationAnimator::Configure(FName InAnimationName, bool bInReverse)
{
	AnimationName = InAnimationName;
	bReverse = bInReverse;
}

void UWidgetAnimationAnimator::SetReverse(bool bInReverse)
{
	bReverse = bInReverse;
}

void UWidgetAnimationAnimator::Play_Implementation(const FOnWidgetAnimatorCompleted& OnCompleted, bool bInstant)
{
	Abort_Implementation();
	const uint64 Session = PlaybackSession;
	Completion = OnCompleted;
	bPending = true;
	if (!ensureMsgf(ParentWidget && Animation, TEXT("Widget animation %s is missing on %s."), *AnimationName.ToString(), *GetNameSafe(ParentWidget)))
	{
		Complete_Implementation(true);
		return;
	}
	const float Length = Animation->GetEndTime() - Animation->GetStartTime();
	if (bInstant || !FMath::IsFinite(Length) || Length <= 0.f)
	{
		ApplyEndpoint(true);
		if (Session == PlaybackSession)
		{
			Complete_Implementation(true);
		}
		return;
	}
	float StartTime = 0.f;
	if (ResumePosition.IsSet())
	{
		const float Position = FMath::Clamp(ResumePosition.GetValue(), 0.f, Length);
		StartTime = bReverse ? Length - Position : Position;
	}
	ResumePosition.Reset();
	CompletionBinding = NewObject<UWidgetAnimationCompletion>(this);
	CompletionBinding->Owner = this;
	CompletionBinding->Session = Session;
	FWidgetAnimationDynamicEvent Finished;
	Finished.BindDynamic(CompletionBinding, &UWidgetAnimationCompletion::Execute);
	const FName UserTag(*FString::Printf(TEXT("WidgetAnimator_%u_%llu"), GetUniqueID(), Session));
	ParentWidget->BindToAnimationEvent(Animation, Finished, EWidgetAnimationEvent::Finished, UserTag);
	const FWidgetAnimationHandle Started = ParentWidget->PlayAnimation(Animation, StartTime, 1,
		bReverse ? EUMGSequencePlayMode::Reverse : EUMGSequencePlayMode::Forward, 1.f, false);
	if (Session == PlaybackSession)
	{
		Playback = Started;
		if (!ensureMsgf(Playback.IsValid(), TEXT("Widget animation %s could not start on %s."), *AnimationName.ToString(), *GetNameSafe(ParentWidget)))
		{
			Complete_Implementation(true);
			return;
		}
		Playback.SetUserTag(UserTag);
	}
}

bool UWidgetAnimationAnimator::Abort_Implementation()
{
	++PlaybackSession;
	const bool bWasPending = bPending;
	if (bWasPending)
	{
		if (const TSharedPtr<FWidgetAnimationState> State = Playback.PinAnimationState())
		{
			ResumePosition = static_cast<float>(State->GetCurrentTime().AsSeconds());
		}
	}
	bPending = false;
	Completion.Unbind();
	UnbindCompletion();
	Pause_Implementation();
	return bWasPending;
}

void UWidgetAnimationAnimator::Pause_Implementation()
{
	if (const TSharedPtr<FWidgetAnimationState> State = Playback.PinAnimationState())
	{
		State->Pause();
	}
}

void UWidgetAnimationAnimator::Reset_Implementation()
{
	Abort_Implementation();
	ResumePosition.Reset();
	ApplyEndpoint(false);
}

void UWidgetAnimationAnimator::Stop_Implementation(bool bComplete)
{
	if (bComplete)
	{
		const uint64 Session = PlaybackSession;
		UnbindCompletion();
		ApplyEndpoint(true);
		if (Session == PlaybackSession)
		{
			Complete_Implementation(false);
		}
		return;
	}
	++PlaybackSession;
	bPending = false;
	Completion.Unbind();
	UnbindCompletion();
	ResumePosition.Reset();
	const FWidgetAnimationHandle Previous = Playback;
	Playback = FWidgetAnimationHandle();
	if (const TSharedPtr<FWidgetAnimationState> State = Previous.PinAnimationState())
	{
		State->Stop();
	}
}

void UWidgetAnimationAnimator::Complete_Implementation(bool bInstant)
{
	if (!bPending)
	{
		return;
	}
	bPending = false;
	ResumePosition.Reset();
	UnbindCompletion();
	const FOnWidgetAnimatorCompleted Finished = Completion;
	Completion.Unbind();
	Finished.ExecuteIfBound(bInstant);
}

void UWidgetAnimationAnimator::OnAnimationCompleted(uint64 InSession)
{
	if (InSession == PlaybackSession)
	{
		Complete_Implementation(false);
	}
}

void UWidgetAnimationAnimator::UnbindCompletion()
{
	if (CompletionBinding)
	{
		CompletionBinding->Owner.Reset();
		if (ParentWidget && Animation)
		{
			FWidgetAnimationDynamicEvent Finished;
			Finished.BindDynamic(CompletionBinding, &UWidgetAnimationCompletion::Execute);
			ParentWidget->UnbindFromAnimationFinished(Animation, Finished);
		}
		CompletionBinding = nullptr;
	}
}

void UWidgetAnimationAnimator::ApplyEndpoint(bool bEnd)
{
	if (!ParentWidget || !Animation)
	{
		return;
	}
	const float Length = Animation->GetEndTime() - Animation->GetStartTime();
	if (!FMath::IsFinite(Length) || Length <= 0.f)
	{
		return;
	}
	const uint64 Session = PlaybackSession;
	const FWidgetAnimationHandle Started = ParentWidget->PlayAnimation(Animation, bEnd != bReverse ? Length : 0.f, 1, EUMGSequencePlayMode::Forward, 1.f, false);
	if (Session == PlaybackSession)
	{
		Playback = Started;
		Pause_Implementation();
		ParentWidget->FlushAnimations();
	}
}
