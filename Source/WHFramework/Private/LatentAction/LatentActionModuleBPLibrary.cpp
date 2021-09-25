// Fill out your copyright notice in the Description page of Project Settings.


#include "LatentAction/LatentActionModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

ALatentActionModule* ULatentActionModuleBPLibrary::LatentActionModuleInst = nullptr;

ALatentActionModule* ULatentActionModuleBPLibrary::GetLatentActionModule(const UObject* InWorldContext)
{
	if (!LatentActionModuleInst || !LatentActionModuleInst->IsValidLowLevel())
    {
    	if(AMainModule* MainModule = UMainModuleBPLibrary::GetMainModule(InWorldContext))
    	{
    		LatentActionModuleInst = MainModule->GetModuleByClass<ALatentActionModule>();
    	}
    }
    return LatentActionModuleInst;
}

void ULatentActionModuleBPLibrary::MoveActorTo(UObject* WorldContextObject, AActor* Actor, ATargetPoint* InTargetPoint, FTransform InTargetTransform, bool bUseRotator, bool bUseScale, float ApplicationTime, bool bEaseIn, bool bEaseOut, float BlendExp, bool bForceShortestRotationPath, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo)
{
	if (UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr)
	{
		if (AMainModule* MainModule = UMainModuleBPLibrary::GetMainModule(WorldContextObject))
		{
			if (ALatentActionModule* LatentActionModule = MainModule->GetModuleByClass<ALatentActionModule>())
			{
				LatentActionModule->MoveActorTo(Actor, InTargetPoint, InTargetTransform, bUseRotator, bUseScale, ApplicationTime, bEaseIn, bEaseOut, BlendExp,  bForceShortestRotationPath, MoveAction, LatentInfo);
			}
		}
	}
}

void ULatentActionModuleBPLibrary::RotatorActorTo(UObject* WorldContextObject, AActor* Actor, FRotator InRotator, float ApplicationTime, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo)
{
	if (UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr)
	{
		if (AMainModule* MainModule = UMainModuleBPLibrary::GetMainModule(WorldContextObject))
		{
			if (ALatentActionModule* LatentActionModule = MainModule->GetModuleByClass<ALatentActionModule>())
			{
				LatentActionModule->RotatorActorTo(Actor, InRotator, ApplicationTime, MoveAction, LatentInfo);
			}
		}
	}
}

void ULatentActionModuleBPLibrary::ScaleActorTo(UObject* WorldContextObject, AActor* Actor, FVector InScale3D, float ApplicationTime, TEnumAsByte<EMoveActorAction::Type> MoveAction, FLatentActionInfo LatentInfo)
{
	if (UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr)
	{
		if (AMainModule* MainModule = UMainModuleBPLibrary::GetMainModule(WorldContextObject))
		{
			if (ALatentActionModule* LatentActionModule = MainModule->GetModuleByClass<ALatentActionModule>())
			{
				LatentActionModule->ScaleActorTo(Actor, InScale3D, ApplicationTime, MoveAction, LatentInfo);
			}
		}
	}	
}

void ULatentActionModuleBPLibrary::DelayPlus(UObject* WorldContextObject, float Duration, TEnumAsByte<EDelayPlusAction::Type> DelayPlusAction, FLatentActionInfo LatentInfo)
{
	if (UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr)
	{
		if (AMainModule* MainModule = UMainModuleBPLibrary::GetMainModule(WorldContextObject))
		{
			if (ALatentActionModule* LatentActionModule = MainModule->GetModuleByClass<ALatentActionModule>())
			{
				LatentActionModule->DelayPlus(WorldContextObject, Duration, DelayPlusAction, LatentInfo);
			}
		}
	}
}
