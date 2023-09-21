// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/AnimationModuleBPLibrary.h"

void UAnimationModuleBPLibrary::MoveActorTo(UObject* WorldContextObject, AActor* Actor, ATargetPoint* TargetPoint, FTransform TargetTransform, bool bUseRotator, bool bUseScale, float DurationTime, bool bEaseIn, bool bEaseOut, float BlendExp, bool bForceShortestRotationPath, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo)
{
	if (AAnimationModule* AnimationModule = AAnimationModule::Get())
	{
		AnimationModule->MoveActorTo(Actor, TargetPoint, TargetTransform, bUseRotator, bUseScale, DurationTime, bEaseIn, bEaseOut, BlendExp,  bForceShortestRotationPath, MoveAction, LatentInfo);
	}
}

void UAnimationModuleBPLibrary::RotatorActorTo(UObject* WorldContextObject, AActor* Actor, FRotator Rotator, float DurationTime, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo)
{
	if (AAnimationModule* AnimationModule = AAnimationModule::Get())
	{
		AnimationModule->RotatorActorTo(Actor, Rotator, DurationTime, MoveAction, LatentInfo);
	}
}

void UAnimationModuleBPLibrary::ScaleActorTo(UObject* WorldContextObject, AActor* Actor, FVector Scale, float DurationTime, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo)
{
	if (AAnimationModule* AnimationModule = AAnimationModule::Get())
	{
		AnimationModule->ScaleActorTo(Actor, Scale, DurationTime, MoveAction, LatentInfo);
	}
}

void UAnimationModuleBPLibrary::CancelableDelay(UObject* WorldContextObject, float Duration, TEnumAsByte<ECancelableDelayAction::Type> CancelableDelayAction, FLatentActionInfo LatentInfo)
{
	if (AAnimationModule* AnimationModule = AAnimationModule::Get())
	{
		AnimationModule->CancelableDelay(WorldContextObject, Duration, CancelableDelayAction, LatentInfo);
	}
}
