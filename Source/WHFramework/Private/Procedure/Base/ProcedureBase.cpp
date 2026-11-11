// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Base/ProcedureBase.h"
#include "Camera/CameraFeatureTypes.h"

#include "Camera/CameraModule.h"
#include "Camera/Anchor/CameraShotAnchor.h"
#include "Camera/Manager/CameraManagerBase.h"
#include "GameFramework/PlayerController.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModule.h"
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

	if (UEventModule* Events = UEventModule::GetPtr(GetWorld())) Events->BroadcastEvent<FEventProcedureStateChanged>(this, {this});
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

	RefreshPlayerCameraActions();

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

	if (UEventModule* Events = UEventModule::GetPtr(GetWorld())) Events->BroadcastEvent<FEventProcedureEntered>(this, {this});
}

void UProcedureBase::OnRefresh()
{
	K2_OnRefresh();
	RefreshPlayerCameraActions();
}

void UProcedureBase::OnGuide()
{
	if (!IsCurrent())
	{
		return;
	}

	TArray<TWeakObjectPtr<APlayerController>> Players;
	PlayerCameras.GetKeys(Players);
	for (const TWeakObjectPtr<APlayerController>& Player : Players) if (Player.IsValid()) ApplyCameraAction(Player.Get());

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
	ClearPlayerCameraActions(bRestoreDefaultCameraOnLeave);
	PlayerTargets.Reset();
	ProcedureState = EProcedureState::Leaved;
	OnStateChanged(ProcedureState);

	WHDebug(FString::Printf(TEXT("离开流程: %s"), *ProcedureDisplayName.ToString()), EDM_All, EDC_Procedure, EDV_Log, FColor::Orange, 5.f);

	K2_OnLeave(InNextProcedure);

	if (UEventModule* Events = UEventModule::GetPtr(GetWorld())) Events->BroadcastEvent<FEventProcedureLeaved>(this, {this});
}


void UProcedureBase::OnShutdown()
{
    ClearPlayerCameraActions(false);
    PlayerTargets.Reset();
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

void UProcedureBase::ApplyCameraAction(APlayerController* InPlayer)
{
	if(!IsCurrent() || !bApplyCameraAction || !::IsValid(InPlayer) || !InPlayer->IsLocalController() || InPlayer->GetWorld() != GetWorld())
	{
		return;
	}

    UCameraModule* CameraModule = UCameraModule::GetPtr(GetWorld());
	ACameraManagerBase* CameraManager = CameraModule ? CameraModule->GetCameraManagerForPlayer(InPlayer) : nullptr;
	if(!CameraManager)
	{
		return;
	}
	FPlayerCameraState& State = PlayerCameras.FindOrAdd(InPlayer);
	if(State.Override.IsValid())
	{
		if (ACameraManagerBase* Previous = State.Manager.Get()) Previous->PopCameraConfigOverride(State.Override);
		State.Override.Reset();
	}
	State.Manager = CameraManager;
	State.Override = CameraManager->PushCameraConfigOverride(CameraOverride, 10);

	if(CameraModeClass)
	{
		FCameraModeContext Context;
		Context.Target = GetOperationTarget(InPlayer, AActor::StaticClass());
		Context.Anchor = CameraAnchor.LoadSynchronous();
		Context.Transition = CameraTransition;
		CameraManager->SetMode(CameraModeClass, Context);
	}
}

AActor* UProcedureBase::GetOperationTarget(APlayerController* InPlayer, TSubclassOf<AActor> InClass) const
{
	if (!::IsValid(InPlayer) || !InPlayer->IsLocalController() || InPlayer->GetWorld() != GetWorld()) return nullptr;
	const TWeakObjectPtr<AActor>* Assigned = PlayerTargets.Find(InPlayer);
	AActor* Target = Assigned ? Assigned->Get() : OperationTarget.LoadSynchronous();
	return Target && (!InClass || Target->IsA(InClass)) ? Target : nullptr;
}

void UProcedureBase::SetOperationTarget(APlayerController* InPlayer, AActor* InOperationTarget, bool bResetCameraView)
{
	if (!::IsValid(InPlayer) || !InPlayer->IsLocalController() || InPlayer->GetWorld() != GetWorld() ||
		(InOperationTarget && InOperationTarget->GetWorld() != GetWorld())) return;
	PlayerTargets.Add(InPlayer, InOperationTarget);
	if(bResetCameraView)
	{
		ApplyCameraAction(InPlayer);
	}
}

void UProcedureBase::RefreshPlayerCameraActions()
{
	UWorld* World = GetWorld();
	if (!bApplyCameraAction || !World || !IsCurrent()) return;
	UCameraModule* Cameras = UCameraModule::GetPtr(World);
	if (!Cameras) return;
	TSet<TWeakObjectPtr<APlayerController>> CurrentPlayers;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		if (!::IsValid(Player) || !Player->IsLocalController() || Player->GetWorld() != World) continue;
		CurrentPlayers.Add(Player);
		const FPlayerCameraState* State = PlayerCameras.Find(Player);
		if (!State || State->Manager.Get() != Cameras->GetCameraManagerForPlayer(Player)) ApplyCameraAction(Player);
	}
	for (auto It = PlayerCameras.CreateIterator(); It; ++It)
		if (!CurrentPlayers.Contains(It.Key()))
		{
			if (ACameraManagerBase* Manager = It.Value().Manager.Get(); Manager && It.Value().Override.IsValid()) Manager->PopCameraConfigOverride(It.Value().Override);
			PlayerTargets.Remove(It.Key());
			It.RemoveCurrent();
		}
}

void UProcedureBase::ClearPlayerCameraActions(bool bRestoreDefault)
{
	const auto PreviousCameras = MoveTemp(PlayerCameras);
	PlayerCameras.Reset();
	for (const auto& Pair : PreviousCameras)
		if (ACameraManagerBase* Manager = Pair.Value.Manager.Get())
		{
			if (Pair.Value.Override.IsValid()) Manager->PopCameraConfigOverride(Pair.Value.Override);
			if (bRestoreDefault) Manager->SetDefaultMode(CameraTransition);
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
