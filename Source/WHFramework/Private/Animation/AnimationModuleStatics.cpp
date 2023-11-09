// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/AnimationModuleStatics.h"

void UAnimationModuleStatics::MoveActorTo(UObject* WorldContextObject, AActor* Actor, ATargetPoint* TargetPoint, FTransform TargetTransform, bool bUseRotator, bool bUseScale, float DurationTime, bool bEaseIn, bool bEaseOut, float BlendExp, bool bForceShortestRotationPath, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo)
{
	if (UAnimationModule* AnimationModule = UAnimationModule::Get())
	{
		AnimationModule->MoveActorTo(Actor, TargetPoint, TargetTransform, bUseRotator, bUseScale, DurationTime, bEaseIn, bEaseOut, BlendExp,  bForceShortestRotationPath, MoveAction, LatentInfo);
	}
}

void UAnimationModuleStatics::RotatorActorTo(UObject* WorldContextObject, AActor* Actor, FRotator Rotator, float DurationTime, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo)
{
	if (UAnimationModule* AnimationModule = UAnimationModule::Get())
	{
		AnimationModule->RotatorActorTo(Actor, Rotator, DurationTime, MoveAction, LatentInfo);
	}
}

void UAnimationModuleStatics::ScaleActorTo(UObject* WorldContextObject, AActor* Actor, FVector Scale, float DurationTime, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo)
{
	if (UAnimationModule* AnimationModule = UAnimationModule::Get())
	{
		AnimationModule->ScaleActorTo(Actor, Scale, DurationTime, MoveAction, LatentInfo);
	}
}

void UAnimationModuleStatics::CancelableDelay(UObject* WorldContextObject, float Duration, TEnumAsByte<ECancelableDelayAction::Type> CancelableDelayAction, FLatentActionInfo LatentInfo)
{
	if (UAnimationModule* AnimationModule = UAnimationModule::Get())
	{
		AnimationModule->CancelableDelay(WorldContextObject, Duration, CancelableDelayAction, LatentInfo);
	}
}
