// Fill out your copyright notice in the Description page of Project Settings.


#include "LatentAction/LatentActionModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

void ULatentActionModuleBPLibrary::MoveActorTo(UObject* WorldContextObject, AActor* Actor, ATargetPoint* InTargetPoint, FTransform InTargetTransform, bool bUseRotator, bool bUseScale, float ApplicationTime, bool bEaseIn, bool bEaseOut, float BlendExp, bool bForceShortestRotationPath, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo)
{
	if (ALatentActionModule* LatentActionModule = ALatentActionModule::Get())
	{
		LatentActionModule->MoveActorTo(Actor, InTargetPoint, InTargetTransform, bUseRotator, bUseScale, ApplicationTime, bEaseIn, bEaseOut, BlendExp,  bForceShortestRotationPath, MoveAction, LatentInfo);
	}
}

void ULatentActionModuleBPLibrary::RotatorActorTo(UObject* WorldContextObject, AActor* Actor, FRotator InRotator, float ApplicationTime, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo)
{
	if (ALatentActionModule* LatentActionModule = ALatentActionModule::Get())
	{
		LatentActionModule->RotatorActorTo(Actor, InRotator, ApplicationTime, MoveAction, LatentInfo);
	}
}

void ULatentActionModuleBPLibrary::ScaleActorTo(UObject* WorldContextObject, AActor* Actor, FVector InScale3D, float ApplicationTime, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo)
{
	if (ALatentActionModule* LatentActionModule = ALatentActionModule::Get())
	{
		LatentActionModule->ScaleActorTo(Actor, InScale3D, ApplicationTime, MoveAction, LatentInfo);
	}
}

void ULatentActionModuleBPLibrary::CancelableDelay(UObject* WorldContextObject, float Duration, TEnumAsByte<ECancelableDelayAction::Type> CancelableDelayAction, FLatentActionInfo LatentInfo)
{
	if (ALatentActionModule* LatentActionModule = ALatentActionModule::Get())
	{
		LatentActionModule->CancelableDelay(WorldContextObject, Duration, CancelableDelayAction, LatentInfo);
	}
}
