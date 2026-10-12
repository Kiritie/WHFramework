// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Base/ProcedureBase.h"

#include "Camera/CameraModule.h"
#include "Camera/Anchor/CameraShotAnchor.h"
#include "Camera/Manager/CameraManagerBase.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Procedure/Event_ProcedureEntered.h"
#include "Event/Events/Procedure/Event_ProcedureLeaved.h"
#include "Event/Events/Procedure/Event_ProcedureStateChanged.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/ProcedureModuleStatics.h"
#include "TimerManager.h"
#include "UObject/UObjectHash.h"


UProcedureBase::UProcedureBase()
{
    ProcedureName = TEXT("ProcedureBase");
    ProcedureDisplayName = NSLOCTEXT("Procedure", "Base", "流程");
}

UWorld* UProcedureBase::GetWorld() const
{
    return RuntimeOwner ? RuntimeOwner->GetWorld() : nullptr;
}

#if WITH_EDITOR
void UProcedureBase::OnGenerate()
{
}

void UProcedureBase::OnUnGenerate()
{
}

void UProcedureBase::PreEditChange(FProperty* PropertyAboutToChange)
{
    Super::PreEditChange(PropertyAboutToChange);
    if (UProcedureAsset* Asset = Cast<UProcedureAsset>(GetOuter()); Asset && !Asset->SourceObject)
    {
        Asset->SetFlags(RF_Transactional);
        Asset->Modify();
        SetFlags(RF_Transactional);
        Modify();
        TArray<UObject*> Objects;
        GetObjectsWithOuter(this, Objects, true);
        for (UObject* Object : Objects)
        {
            Object->SetFlags(RF_Transactional);
            Object->Modify();
        }
    }
}

void UProcedureBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    if (UProcedureAsset* Asset = Cast<UProcedureAsset>(GetOuter()))
    {
        Asset->Modify();
        Asset->Recompile();
    }
}
#endif

void UProcedureBase::OnStateChanged(EProcedureState InProcedureState)
{
	OnProcedureStateChanged.Broadcast(InProcedureState);
	K2_OnStateChanged(InProcedureState);

	UEventModuleStatics::BroadcastEvent<FEventProcedureStateChanged>(this, {this});
}

void UProcedureBase::OnInitialize()
{
	K2_OnInitialize();

}

void UProcedureBase::OnEnter(UProcedureBase* InLastProcedure)
{
	ProcedureState = EProcedureState::Entered;
	OnStateChanged(ProcedureState);

	WHDebug(FString::Printf(TEXT("进入流程: %s"), *ProcedureDisplayName.ToString()), EDM_All, EDC_Procedure, EDV_Log, FColor::Cyan, 5.f);

	K2_OnEnter(InLastProcedure);

	ApplyCameraAction();

	switch(ProcedureGuideType)
	{
		case EProcedureGuideType::TimerOnce:
		case EProcedureGuideType::TimerLoop:
		{
			ScheduleGuide();
			break;
		}
		default: break;
	}

	UEventModuleStatics::BroadcastEvent<FEventProcedureEntered>(this, {this});
}

void UProcedureBase::OnRefresh()
{
	K2_OnRefresh();
}

void UProcedureBase::OnGuide()
{
	if (!IsCurrent())
	{
		return;
	}

	ApplyCameraAction();

	switch(ProcedureGuideType)
	{
		case EProcedureGuideType::TimerLoop:
		{
			ScheduleGuide();
			break;
		}
		default: break;
	}

	K2_OnGuide();
}

void UProcedureBase::OnLeave(UProcedureBase* InNextProcedure)
{
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(StartGuideTimerHandle);
    }
    UCameraModule* CameraModule = UCameraModule::GetPtr(World);
    ACameraManagerBase* CameraManager = CameraModule ? CameraModule->GetCameraManager() : nullptr;
    if (CameraOverrideHandle.IsValid())
    {
        if (CameraManager)
        {
            CameraManager->PopCameraConfigOverride(CameraOverrideHandle);
        }
        CameraOverrideHandle.Reset();
    }
	ProcedureState = EProcedureState::Leaved;
	OnStateChanged(ProcedureState);

    if (bRestoreDefaultCameraOnLeave && CameraManager)
    {
        CameraManager->SetDefaultMode(CameraTransition);
    }

	WHDebug(FString::Printf(TEXT("离开流程: %s"), *ProcedureDisplayName.ToString()), EDM_All, EDC_Procedure, EDV_Log, FColor::Orange, 5.f);

	K2_OnLeave(InNextProcedure);

	UEventModuleStatics::BroadcastEvent<FEventProcedureLeaved>(this, {this});
}


void UProcedureBase::OnShutdown()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(StartGuideTimerHandle);
    }
}

void UProcedureBase::Guide_Implementation()
{
    if (IsCurrent())
    {
        OnGuide();
    }
}

bool UProcedureBase::IsCurrent_Implementation()
{
    return RuntimeOwner && RuntimeOwner->IsCurrentProcedure(this);
}

bool UProcedureBase::Advance(FName Output)
{
    return RuntimeOwner && RuntimeOwner->AdvanceFrom(Activation, Output);
}

bool UProcedureBase::CanAdvance(FName Output) const
{
    return RuntimeOwner && RuntimeOwner->IsCurrentProcedure(this) && RuntimeOwner->CanAdvance(Output);
}

FProcedureActivationToken UProcedureBase::CaptureActivation() const
{
    return Activation;
}

UProcedureModule* UProcedureBase::GetProcedureModule() const
{
    return RuntimeOwner;
}

UProcedureAsset* UProcedureBase::GetProcedureAsset() const
{
    return RuntimeOwner ? RuntimeOwner->GetCurrentAsset() : Cast<UProcedureAsset>(GetOuter());
}

void UProcedureBase::ApplyCameraAction()
{
	if(!IsCurrent() || !bApplyCameraAction)
	{
		return;
	}

    UCameraModule* CameraModule = UCameraModule::GetPtr(GetWorld());
    ACameraManagerBase* CameraManager = CameraModule ? CameraModule->GetCameraManager() : nullptr;
	if(!CameraManager)
	{
		return;
	}
	if(CameraOverrideHandle.IsValid())
	{
		CameraManager->PopCameraConfigOverride(CameraOverrideHandle);
		CameraOverrideHandle.Reset();
	}
	CameraOverrideHandle = CameraManager->PushCameraConfigOverride(CameraOverride, 10);

	if(CameraModeClass)
	{
		FCameraModeContext Context;
		Context.Target = OperationTarget.LoadSynchronous();
		Context.Anchor = CameraAnchor.LoadSynchronous();
		Context.Transition = CameraTransition;
		CameraManager->SetMode(CameraModeClass, Context);
	}
}

AActor* UProcedureBase::GetOperationTarget(TSubclassOf<AActor> InClass) const
{
	AActor* Target = OperationTarget.Get();
	return Target && (!InClass || Target->IsA(InClass)) ? Target : nullptr;
}

void UProcedureBase::SetOperationTarget(AActor* InOperationTarget, bool bResetCameraView)
{
	OperationTarget = InOperationTarget;
	if(bResetCameraView)
	{
		ApplyCameraAction();
	}
}

void UProcedureBase::Switch_Implementation()
{
    if (RuntimeOwner)
    {
        RuntimeOwner->SwitchProcedure(this);
    }
}

void UProcedureBase::UnSwitch_Implementation()
{
    if (IsCurrent())
    {
        RuntimeOwner->SwitchProcedure(nullptr);
    }
}

void UProcedureBase::SwitchLast_Implementation()
{
    if (IsCurrent())
    {
        RuntimeOwner->SwitchLastProcedure();
    }
}

void UProcedureBase::SwitchNext_Implementation()
{
    if (IsCurrent())
    {
        RuntimeOwner->SwitchNextProcedure();
    }
}

void UProcedureBase::SwitchPre()
{
    SwitchLast();
}

void UProcedureBase::ScheduleGuide()
{
    UProcedureModule* Module = GetProcedureModule();
    UWorld* World = GetWorld();
    if (!Module || !World || !IsCurrent())
    {
        return;
    }
    const FProcedureActivationToken Token = CaptureActivation();
    const TWeakObjectPtr<UProcedureModule> WeakModule(Module);
    FTimerDelegate Callback = FTimerDelegate::CreateWeakLambda(this, [WeakModule, Token]
    {
        if (UProcedureModule* Owner = WeakModule.Get(); Owner && Owner->CaptureActivation() == Token)
        {
            Owner->GuideCurrentProcedure();
        }
    });
    World->GetTimerManager().SetTimer(StartGuideTimerHandle, Callback, FMath::Max(0.01f, ProcedureGuideIntervalTime), false);
}
