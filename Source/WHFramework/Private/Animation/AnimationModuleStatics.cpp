// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/AnimationModuleStatics.h"

void UAnimationModuleStatics::CancelableDelay(float Duration, TEnumAsByte<ECancelableDelayAction::Type> CancelableDelayAction, FLatentActionInfo LatentInfo)
{
	UAnimationModule::Get().CancelableDelay(Duration, CancelableDelayAction, LatentInfo);
}

void UAnimationModuleStatics::ExecuteWithDelay(float Duration, TFunction<void()> OnFinish)
{
	UAnimationModule::Get().ExecuteWithDelay(Duration, OnFinish);
}

void UAnimationModuleStatics::ExecuteWithTransition(float Duration, TFunction<void()> OnFinish)
{
	UAnimationModule::Get().ExecuteWithTransition(Duration, OnFinish);
}
