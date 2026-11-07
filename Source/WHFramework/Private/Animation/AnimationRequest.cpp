#include "Animation/AnimationRequest.h"

#include "Animation/AnimationModule.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UAnimationRequest::Initialize(UAnimationModule& InModule, UWorld& InWorld, UObject& InOwner, const FGuid& InId, TFunction<void()> InFinish, TFunction<void()> InCancel)
{
	Module = &InModule;
	World = &InWorld;
	Owner = &InOwner;
	Id = InId;
	Finish = MoveTemp(InFinish);
	Canceled = MoveTemp(InCancel);
}

void UAnimationRequest::Cancel()
{
	if (bFinished) return;
	bFinished = true;
	TFunction<void()> Callback = MoveTemp(Canceled);
	Finish = nullptr;
	if (UAnimationModule* Current = Module.Get()) Current->FinishRequest(Id, this);
	UObject* OriginalOwner = Owner.Get();
	if (OriginalOwner && Callback) Callback();
}

void UAnimationRequest::Complete(int32 Linkage)
{
	if (bFinished || bDispatchPending) return;
	UWorld* OriginalWorld = World.Get();
	UObject* OriginalOwner = Owner.Get();
	UAnimationModule* Current = Module.Get();
	if (!Current || !Current->bAcceptingRequests || !OriginalWorld || !OriginalOwner)
	{
		Cancel();
		return;
	}
	if (!Current->QueueRequestCompletion(Id, this)) { Cancel(); return; }
	bDispatchPending = true;
	OriginalWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { Dispatch(); }));
}

void UAnimationRequest::Dispatch()
{
	if (bFinished || !bDispatchPending) return;
	UAnimationModule* Current = Module.Get();
	UObject* OriginalOwner = Owner.Get();
	if (!Current || !Current->bAcceptingRequests || !OriginalOwner)
	{
		Cancel();
		return;
	}
	bFinished = true;
	bDispatchPending = false;
	TFunction<void()> Callback = MoveTemp(Finish);
	Canceled = nullptr;
	Current->FinishRequest(Id, this);
	if (Callback) Callback();
}
