// Copyright 2023 Chang Qing,. All Rights Reserved.

#include "Animation/AsyncAction/AsyncAction_ExecuteWithTransition.h"

#include "Animation/AnimationModuleStatics.h"
#include "Animation/AnimationModule.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UAsyncAction_ExecuteWithTransition::UAsyncAction_ExecuteWithTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAsyncAction_ExecuteWithTransition* UAsyncAction_ExecuteWithTransition::ExecuteWithTransition(const UObject* WorldContextObject, APlayerController* InPlayer, float Duration)
{
	UWorld* World = GEngine && WorldContextObject ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || World->bIsTearingDown || !IsValid(InPlayer) || !InPlayer->IsLocalController() || InPlayer->GetWorld() != World) return nullptr;
	UAsyncAction_ExecuteWithTransition* Task = NewObject<UAsyncAction_ExecuteWithTransition>(World);
	Task->RequestWorld = World;
	Task->RequestPlayer = InPlayer;
	Task->RequestDuration = Duration;
	Task->RegisterWithGameInstance(World);
	return Task;
}

void UAsyncAction_ExecuteWithTransition::Activate()
{
	if (bActivated) return;
	bActivated = true;
	if (bCanceled) { SetReadyToDestroy(); return; }
	UWorld* World = RequestWorld.Get();
	UAnimationModule* Module = World && !World->bIsTearingDown ? UAnimationModule::GetPtr(World) : nullptr;
	APlayerController* Player = RequestPlayer.Get();
	if (!Module || !Player || Player->GetWorld() != World || !Player->IsLocalController()) { SetReadyToDestroy(); return; }
	RequestModule = Module;
	const TWeakObjectPtr<UAsyncAction_ExecuteWithTransition> WeakTask(this);
	RequestId = UAnimationModuleStatics::ExecuteWithTransition(World, Player, this, RequestDuration,
		[WeakTask]()
		{
			if (UAsyncAction_ExecuteWithTransition* Task = WeakTask.Get())
			{
				Task->RequestId.Invalidate();
				Task->OnFinish.Broadcast();
				Task->SetReadyToDestroy();
			}
		},
		[WeakTask]()
		{
			if (UAsyncAction_ExecuteWithTransition* Task = WeakTask.Get())
			{
				Task->bCanceled = true;
				Task->RequestId.Invalidate();
				Task->OnCanceled.Broadcast();
				Task->SetReadyToDestroy();
			}
		});
	if (bCanceled && RequestId.IsValid()) Module->CancelRequest(RequestId);
	if (!RequestId.IsValid()) SetReadyToDestroy();
}

void UAsyncAction_ExecuteWithTransition::Cancel()
{
	bCanceled = true;
	if (UAnimationModule* Module = RequestModule.Get(); Module && RequestId.IsValid() && Module->CancelRequest(RequestId)) return;
	RequestId.Invalidate();
	SetReadyToDestroy();
}

void UAsyncAction_ExecuteWithTransition::BeginDestroy()
{
	if (UAnimationModule* Module = RequestModule.Get(); Module && RequestId.IsValid()) Module->CancelRequest(RequestId);
	RequestId.Invalidate();
	Super::BeginDestroy();
}
