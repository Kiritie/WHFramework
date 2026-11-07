// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/TaskModule.h"
#include "WHFrameworkCoreMacros.h"
#include "Task/TaskReferenceTypes.h"
#include "Task/TaskSaveTypes.h"
#include "Common/LifecycleTypes.h"

#include "Main/MainModule.h"
#include "Common/CommonModuleStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Task/Base/TaskBase.h"
#include "Character/CharacterModuleTypes.h"
#include "Event/EventModule.h"
#include "Event/Events/Task/Event_CurrentTaskChanged.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "Scene/SceneModule.h"
#include "Task/TaskModuleNetworkComponent.h"
#include "Misc/Crc.h"
#include "Misc/ScopeExit.h"
#include "Misc/SecureHash.h"

IMPLEMENTATION_MODULE(UTaskModule)

// ParamSets default values
UTaskModule::UTaskModule()
{
	ModuleName = FName("TaskModule");
	ModuleDisplayName = FText::FromString(TEXT("Task Module"));
	SaveDataVersion = 2;
	ModuleDependencies.AddUnique(TEXT("EventModule"));
	SaveScope = ESaveScope::World;


	ModuleNetworkComponent = UTaskModuleNetworkComponent::StaticClass();

	Assets = TArray<UTaskAsset*>();
}

UTaskModule::~UTaskModule()
{
}

#if WITH_EDITOR
void UTaskModule::OnGenerate()
{
	Super::OnGenerate();
}

void UTaskModule::OnDestroy()
{
	Super::OnDestroy();

}
#endif

void UTaskModule::OnInitialize()
{
	SaveDataVersion = 2;
	Super::OnInitialize();

	for(auto Iter : DefaultAssets)
	{
		AddAsset(Iter);
	}
}

void UTaskModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UTaskModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
    Super::OnRefresh(DeltaSeconds, bInEditor);
    if (bInEditor || bLoadingTasks || bClearingRuntimeAssets) return;
    BeginTaskMutation();
    ON_SCOPE_EXIT { EndTaskMutation(); };
    for (UTaskAsset* Asset : Assets)
    {
        if (!Asset || Asset->bPresentationOnly) continue;
        for (UTaskBase* Task : Asset->RootTasks) if (Task) RefreshTask(Task);
    }
}

void UTaskModule::OnPause()
{
	Super::OnPause();
	for (UTaskAsset* Asset : Assets) for (const auto& Pair : Asset->TaskMap) Pair.Value->SetTaskTimersPaused(true);
}

void UTaskModule::OnUnPause()
{
	Super::OnUnPause();
	for (UTaskAsset* Asset : Assets) for (const auto& Pair : Asset->TaskMap) Pair.Value->SetTaskTimersPaused(false);
}

void UTaskModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
	if (PHASEC(InPhase, EPhase::Final))
	{
		FString Error;
		if (!FinalizeTaskTermination(Error)) UE_LOG(LogTemp, Error, TEXT("Task termination failed: %s"), *Error);
	}
}

bool UTaskModule::FinalizeTaskTermination(FString& OutError)
{
	if (bTaskTerminationFinalized) return true;
	if (bTaskTerminationFinalizing || bClearingRuntimeAssets || TaskMutationDepth != 0)
	{
		OutError = TEXT("Task.TerminationStillInProgress");
		return false;
	}
	TGuardValue<bool> Finalizing(bTaskTerminationFinalizing, true);
	if (!PrepareTaskTermination(OutError)) return false;
	TGuardValue<bool> Clearing(bClearingRuntimeAssets, true);
	for (UTaskAsset* Asset : Assets)
    {
        if (!Asset) continue;
        for (const auto& Pair : Asset->TaskMap) if (Pair.Value) Pair.Value->ReleaseRuntimeResources();
    }
    CurrentTasks.Reset();
    Assets.Reset();
    PreparedRuntimeAssets.Reset();
    PendingResume.Reset();
    TaskMarkerIDs.Reset();
	bTaskTerminationFinalized = true;
	return true;
}

FString UTaskModule::GetSaveKey(const UTaskAsset* Asset, const FString& GUID)
{
	const UObject* Source = Asset && Asset->SourceObject ? Asset->SourceObject : Asset;
	return FString::Printf(TEXT("%s:%s:%s"), Source ? *Source->GetPathName() : TEXT(""), Asset ? *Asset->InstanceID.ToString(EGuidFormats::Digits) : TEXT(""), *GUID);
}

FGuid UTaskModule::MakeTaskMarkerID(const UTaskAsset* Asset, const FString& GUID)
{
	const FString Key = GetSaveKey(Asset, GUID);
	return FGuid(FCrc::StrCrc32(*Key), FCrc::StrCrc32(*(Key + TEXT(".B"))), FCrc::StrCrc32(*(Key + TEXT(".C"))), FCrc::StrCrc32(*(Key + TEXT(".D"))));
}

bool UTaskModule::ClearRuntimeAssets()
{
    if (bClearingRuntimeAssets) return false;
    TGuardValue<bool> ClearingGuard(bClearingRuntimeAssets, true);
    if (!RemoveTaskMarkers()) return false;
    for (UTaskAsset* Asset : Assets)
    {
        if (!Asset) continue;
        for (const auto& Pair : Asset->TaskMap) if (Pair.Value) Pair.Value->OnSuspend();
    }
    TArray<FGuid> PreviousOwners;
    CurrentTasks.GetKeys(PreviousOwners);
    CurrentTasks.Reset();
    Assets.Reset();
    PreparedRuntimeAssets.Reset();
    PendingResume.Reset();
    bTaskMarkersDirty = false;
    bAutomaticTaskEntryDirty = false;
    if (UEventModule* Events = UEventModule::GetPtr(GetWorld()))
        for (const FGuid& Owner : PreviousOwners) Events->BroadcastEvent<FEventCurrentTaskChanged>(this, {Owner, nullptr});
    return true;
}

void UTaskModule::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
    bTaskLoadSucceeded = false;
    const FTaskModuleSaveData* SavedData = InSaveData.GetPtr<FTaskModuleSaveData>();
    if (!SavedData) return;
    const FTaskModuleSaveData& Data = *SavedData;
    if (!Data.IsSaved()) { bTaskLoadSucceeded = true; return; }
    if (PHASEC(InPhase, EPhase::Primary))
    {
        bLoadingTasks = true;
        if (!ClearRuntimeAssets()) return;
        for (const FTaskAssetSaveData& AssetData : Data.Assets)
        {
            UTaskAsset* Source = AssetData.Asset.LoadSynchronous();
            if (!Source)
            {
                UE_LOG(LogTemp, Error, TEXT("Missing task asset in save: %s"), *AssetData.Asset.ToString());
                return;
            }
            UTaskAsset* RuntimeAsset = AddAssetInternal(Source, AssetData.InstanceID, AssetData.AgentID, AssetData.OwnerActorID);
            if (!RuntimeAsset) return;
            RuntimeAsset->AgentLocation = AssetData.AgentLocation;
            RuntimeAsset->InstanceKey = AssetData.InstanceKey;
        }
        for (UTaskAsset* Source : DefaultAssets)
            if (Source && !bDefaultAssetsPerOwner && !AddAsset(Source)) return;
        TSet<FString> RestoredKeys;
        for (UTaskAsset* Asset : Assets)
        {
            if (!Asset) continue;
            for (const auto& Pair : Asset->TaskMap)
            {
                UTaskBase* Task = Pair.Value;
                const FString Key = GetSaveKey(Asset, Pair.Key);
                if (const FTaskRuntimeSaveData* Record = Data.TaskRecords.Find(Key))
                {
                    if (!Task || RestoredKeys.Contains(Key) || !Task->LoadSaveData(Record->Data, EPhase::Primary)) return;
                    PendingResume.Add(Task, *Record);
                    RestoredKeys.Add(Key);
                }
            }
        }
        if (RestoredKeys.Num() != Data.TaskRecords.Num()) return;
    }
    if (PHASEC(InPhase, EPhase::Final))
    {
        const auto Resume = PendingResume;
        PendingResume.Reset();
        for (const auto& Pair : Resume)
        {
            UTaskBase* Task = Pair.Key;
            if (!Task || !Task->LoadSaveData(Pair.Value.Data, EPhase::Final) || !Task->ResumeRuntimeData(Pair.Value)) return;
            Task->SetTaskTimersPaused(ModuleState == EModuleState::Paused);
        }
        bLoadingTasks = false;
        RequestTaskMarkersRefresh();
    }
    bTaskLoadSucceeded = true;
}

void UTaskModule::UnloadData(EPhase InPhase)
{
	if (PHASEC(InPhase, EPhase::Primary))
	{
		if (ClearRuntimeAssets()) bLoadingTasks = false;
		else MarkUnloadDataFailed();
	}
}

FParameter UTaskModule::ToData()
{
    FTaskModuleSaveData SaveData;
    for (UTaskAsset* Asset : Assets)
    {
        if (!Asset || !Asset->SourceObject || Asset->bPresentationOnly)
        {
            continue;
        }
        FTaskAssetSaveData AssetData;
        AssetData.Asset = Cast<UTaskAsset>(Asset->SourceObject);
        AssetData.InstanceID = Asset->InstanceID;
        AssetData.AgentID = Asset->AgentID;
        AssetData.OwnerActorID = Asset->OwnerActorID;
        AssetData.InstanceKey = Asset->InstanceKey;
        AssetData.AgentLocation = Asset->AgentLocation;
        SaveData.Assets.Add(AssetData);
        for (const auto& Pair : Asset->TaskMap)
        {
            if (Pair.Value)
            {
                SaveData.TaskRecords.Add(GetSaveKey(Asset, Pair.Key), Pair.Value->CaptureRuntimeData());
            }
        }
    }
    return FParameter(MoveTemp(SaveData));
}

FString UTaskModule::GetModuleDebugMessage()
{
	return FString::Printf(TEXT("Tracked player tasks: %d"), CurrentTasks.Num());
}
