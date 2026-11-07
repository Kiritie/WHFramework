// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/AnimationModuleStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void UAnimationModuleStatics::CancelableDelay(const UObject* WorldContextObject, float Duration, TEnumAsByte<ECancelableDelayAction::Type> CancelableDelayAction, FLatentActionInfo LatentInfo)
{
	UWorld* World = GEngine && WorldContextObject ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (UAnimationModule* Module = World ? UAnimationModule::GetPtr(World) : nullptr)
		Module->CancelableDelay(Duration, CancelableDelayAction, LatentInfo);
}

FGuid UAnimationModuleStatics::ExecuteWithDelay(const UObject* WorldContextObject, UObject* InOwner, float Duration, TFunction<void()> OnFinish, TFunction<void()> OnCancel)
{
	UWorld* World = GEngine && WorldContextObject ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	UAnimationModule* Module = World ? UAnimationModule::GetPtr(World) : nullptr;
	return Module ? Module->ExecuteWithDelay(InOwner, Duration, MoveTemp(OnFinish), MoveTemp(OnCancel)) : FGuid();
}

FGuid UAnimationModuleStatics::ExecuteWithTransition(const UObject* WorldContextObject, APlayerController* InPlayer, UObject* InOwner, float Duration, TFunction<void()> OnFinish, TFunction<void()> OnCancel)
{
	UWorld* World = GEngine && WorldContextObject ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	UAnimationModule* Module = World ? UAnimationModule::GetPtr(World) : nullptr;
	return Module ? Module->ExecuteWithTransition(InPlayer, InOwner, Duration, MoveTemp(OnFinish), MoveTemp(OnCancel)) : FGuid();
}
