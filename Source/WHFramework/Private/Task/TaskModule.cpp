// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/TaskModule.h"

#include "Main/MainModule.h"
#include "Task/Base/TaskBase.h"
#include "Character/CharacterModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Task/EventHandle_CurrentTaskChanged.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/TaskSaveGame.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "Scene/SceneModule.h"
#include "Task/TaskModuleNetworkComponent.h"
#include "Misc/Crc.h"

IMPLEMENTATION_MODULE(UTaskModule)

// ParamSets default values
UTaskModule::UTaskModule()
{
	ModuleName = FName("TaskModule");
	ModuleDisplayName = FText::FromString(TEXT("Task Module"));

	ModuleSaveGame = UTaskSaveGame::StaticClass();

	ModuleNetworkComponent = UTaskModuleNetworkComponent::StaticClass();

	Assets = TArray<UTaskAsset*>();
	CurrentTask = nullptr;
}

UTaskModule::~UTaskModule()
{
	TERMINATION_MODULE(UTaskModule)
}

#if WITH_EDITOR
void UTaskModule::OnGenerate()
{
	Super::OnGenerate();
}

void UTaskModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UTaskModule)
}
#endif

void UTaskModule::OnInitialize()
{
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

	if(bInEditor) return;

	if (bLoadingTasks) return;
	const TArray<UTaskAsset*> Snapshot = Assets;
	for (UTaskAsset* Asset : Snapshot)
	{
		if (!Asset || !Assets.Contains(Asset)) continue;
		const TArray<UTaskBase*> Roots = Asset->RootTasks;
		for (UTaskBase* Task : Roots)
		{
			if (!Task || Task->IsCompleted()) continue;
			const bool bSelectNextTask = !CurrentTask || CurrentTask->IsCompleted();
			if (Task->TaskEnterType == ETaskEnterType::Automatic)
				EnterTask(Task, bSelectNextTask);
			if (Task->IsEntered()) EnterTask(Task, bSelectNextTask);
			if (Assets.Contains(Asset)) RefreshTask(Task);
		}
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
	if (PHASEC(InPhase, EPhase::Final)) ClearRuntimeAssets();
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

void UTaskModule::ClearRuntimeAssets()
{
	RemoveTaskMarkers();
	for (UTaskAsset* Asset : Assets)
		if (Asset) for (const auto& Pair : Asset->TaskMap)
			if (Pair.Value) Pair.Value->OnSuspend();
	const bool bHadCurrentTask = CurrentTask != nullptr;
	CurrentTask = nullptr;
	Assets.Reset();
	PendingResume.Reset();
	if(bHadCurrentTask) UEventModuleStatics::BroadcastEvent(UEventHandle_CurrentTaskChanged::StaticClass(), this, {FParameter(static_cast<UObject*>(nullptr))});
}

void UTaskModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	if (!InSaveData) return;
	auto& Data = InSaveData->CastRef<FTaskModuleSaveData>();
	if (!Data.IsSaved()) return;
	if (PHASEC(InPhase, EPhase::Primary))
	{
		bLoadingTasks = true;
		ClearRuntimeAssets();
		for (const FTaskAssetSaveData& AssetData : Data.Assets)
		{
			if (UTaskAsset* Source = AssetData.Asset.LoadSynchronous())
			{
				if(UTaskAsset* RuntimeAsset = AddAssetInternal(Source, AssetData.InstanceID, AssetData.AgentID))
				{
					RuntimeAsset->AgentLocation = AssetData.AgentLocation;
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Missing task asset in save: %s"), *AssetData.Asset.ToString());
			}
		}
		for (UTaskAsset* Source : DefaultAssets) AddAsset(Source);
		for (UTaskAsset* Asset : Assets)
		{
			for (const auto& Pair : Asset->TaskMap)
			{
				UTaskBase* Task = Pair.Value;
				if (const FTaskRuntimeSaveData* Record = Data.TaskRecords.Find(GetSaveKey(Asset, Pair.Key)))
				{
					Task->LoadSaveData(const_cast<FSaveData*>(&Record->Archive));
					PendingResume.Add(Task, *Record);
				}

			}
		}
		CurrentTask = ResolveTask(Data.CurrentTask);
		if (CurrentTask && CurrentTask->IsLeaved()) CurrentTask = nullptr;
	}
	if (PHASEC(InPhase, EPhase::Final))
	{
		const auto Resume = PendingResume;
		PendingResume.Reset();
		for (const auto& Pair : Resume)
			if (Pair.Key && Assets.Contains(Pair.Key->GetTaskAsset()))
			{
				Pair.Key->ResumeRuntimeData(Pair.Value);
				Pair.Key->SetTaskTimersPaused(ModuleState == EModuleState::Paused);
			}
		bLoadingTasks = false;
		RefreshTaskMarkers();
		UEventModuleStatics::BroadcastEvent(UEventHandle_CurrentTaskChanged::StaticClass(), this, {CurrentTask});
	}
}

void UTaskModule::UnloadData(EPhase InPhase)
{
	if (PHASEC(InPhase, EPhase::Primary))
	{
		ClearRuntimeAssets();
		bLoadingTasks = false;
	}
}

FSaveData* UTaskModule::ToData()
{
	CachedSaveData = FTaskModuleSaveData();
	for (UTaskAsset* Asset : Assets)
	{
		if (!Asset || !Asset->SourceObject) continue;
		FTaskAssetSaveData AssetData;
		AssetData.Asset = Cast<UTaskAsset>(Asset->SourceObject);
		AssetData.InstanceID = Asset->InstanceID;
		AssetData.AgentID = Asset->AgentID;
		AssetData.AgentLocation = Asset->AgentLocation;
		CachedSaveData.Assets.Add(AssetData);
		for (const auto& Pair : Asset->TaskMap)
			if (Pair.Value) CachedSaveData.TaskRecords.Add(GetSaveKey(Asset, Pair.Key), Pair.Value->CaptureRuntimeData());
	}
	if (CurrentTask && CurrentTask->GetTaskAsset())
	{
		CachedSaveData.CurrentTask = CurrentTask->GetTaskAsset()->MakeTaskReference(CurrentTask->TaskGUID);
	}
	return &CachedSaveData;
}

FString UTaskModule::GetModuleDebugMessage()
{
	return FString::Printf(TEXT("CurrentTask: %s"), CurrentTask ? *CurrentTask->TaskDisplayName.ToString() : TEXT("None"));
}

UTaskAsset* UTaskModule::GetAsset(UTaskAsset* InAsset) const
{
	if(Assets.Contains(InAsset)) return InAsset;
	return GetAssetByInstance(InAsset, FGuid());
}

UTaskAsset* UTaskModule::GetAssetByInstance(UTaskAsset* InAsset, FGuid InInstanceID) const
{
	if(!InAsset) return nullptr;
	const UObject* Source = InAsset->SourceObject ? InAsset->SourceObject : InAsset;
	for(UTaskAsset* Asset : Assets)
	{
		if(Asset && (Asset == InAsset || Asset->SourceObject == Source) && Asset->InstanceID == InInstanceID) return Asset;
	}
	return nullptr;
}

TArray<UTaskAsset*> UTaskModule::GetAssetsByAgent(FGuid InAgentID) const
{
	TArray<UTaskAsset*> Result;
	if(!InAgentID.IsValid()) return Result;
	for(UTaskAsset* Asset : Assets)
	{
		if(Asset && Asset->AgentID == InAgentID) Result.Add(Asset);
	}
	return Result;
}

UTaskBase* UTaskModule::ResolveRuntimeTask(UTaskBase* Task) const
{
	if (!Task) return nullptr;
	UTaskAsset* TaskAsset = Task->GetTaskAsset();
	if(Assets.Contains(TaskAsset)) return Task;
	if (UTaskAsset* Asset = GetAsset(TaskAsset)) return Asset->TaskMap.FindRef(Task->TaskGUID);
	return nullptr;
}

UTaskAsset* UTaskModule::AddAsset(UTaskAsset* InAsset)
{
	return AddAssetInternal(InAsset, FGuid(), FGuid());
}

UTaskAsset* UTaskModule::CreateAsset(UTaskAsset* InAsset, FGuid InAgentID)
{
	if(!InAsset || !InAgentID.IsValid()) return nullptr;
	FGuid InstanceID = FGuid::NewGuid();
	while(GetAssetByInstance(InAsset, InstanceID)) InstanceID = FGuid::NewGuid();
	return AddAssetInternal(InAsset, InstanceID, InAgentID);
}

UTaskAsset* UTaskModule::AddAssetInternal(UTaskAsset* InAsset, FGuid InInstanceID, FGuid InAgentID)
{
	if(!InAsset) return nullptr;
	UTaskAsset* Source = Cast<UTaskAsset>(InAsset->SourceObject ? InAsset->SourceObject : InAsset);
	if(!Source) return nullptr;
	if(UTaskAsset* Existing = GetAssetByInstance(Source, InInstanceID)) return Existing;
	TArray<FText> Errors;
	if(!Source->ValidateTasks(Errors)) return nullptr;
	UTaskAsset* RuntimeAsset = DuplicateObject<UTaskAsset>(Source, this);
	RuntimeAsset->SourceObject = Source;
	RuntimeAsset->InstanceID = InInstanceID;
	RuntimeAsset->AgentID = InAgentID;
	TArray<FText> ExpansionErrors;
	if(!RuntimeAsset->ExpandTaskAssetReferences(ExpansionErrors))
	{
		for(const FText& Error : ExpansionErrors) UE_LOG(LogTemp, Warning, TEXT("%s"), *Error.ToString());
		return nullptr;
	}
	if(!RuntimeAsset->RebuildTaskMap()) return nullptr;
	Assets.Add(RuntimeAsset);
	RuntimeAsset->Initialize();
	if(InInstanceID.IsValid())
	{
		for(const auto& Pair : RuntimeAsset->TaskMap)
		{
			UTaskBase* Task = Pair.Value;
			if(!Task) continue;
			for(FTaskObjective& Objective : Task->Objectives)
			{
				if(Objective.RequiredCountRange == FIntPoint::ZeroValue) continue;
				const uint32 Seed = HashCombineFast(GetTypeHash(InInstanceID), HashCombineFast(GetTypeHash(Pair.Key), GetTypeHash(Objective.ObjectiveID)));
				Objective.RequiredCount = FRandomStream(Seed).RandRange(Objective.RequiredCountRange.X, Objective.RequiredCountRange.Y);
			}
		}
	}
	if(!bLoadingTasks)
	{
		RefreshTaskMarkers();
	}
	return RuntimeAsset;
}

void UTaskModule::RemoveAsset(UTaskAsset* InAsset)
{
	if (UTaskAsset* Asset = GetAsset(InAsset))
	{
		for (const auto& Pair : Asset->TaskMap)
		{
			if (Pair.Value) Pair.Value->OnSuspend();
			PendingResume.Remove(Pair.Value);
		}
		const bool bClearCurrentTask = CurrentTask && CurrentTask->GetTaskAsset() == Asset;
		Assets.Remove(Asset);
		if (bClearCurrentTask) SetCurrentTask(nullptr);
		if (!bLoadingTasks)
		{
			RefreshTaskMarkers();
		}
	}
}

void UTaskModule::RestoreTask(UTaskBase* InTask)
{
	InTask = ResolveRuntimeTask(InTask);
	if(InTask && InTask->GetTaskState() != ETaskState::None)
	{
		if (CurrentTask == InTask || (CurrentTask && InTask->IsParentOf(CurrentTask))) SetCurrentTask(nullptr);
		InTask->OnRestore();
		RefreshTaskMarkers();
	}
}

void UTaskModule::RestoreTaskByGUID(const FString& InTaskGUID)
{
	RestoreTask(GetTaskByGUID(InTaskGUID));
}

void UTaskModule::EnterTask(UTaskBase* InTask, bool bSetAsCurrent)
{
	InTask = ResolveRuntimeTask(InTask);
	if (!InTask || InTask->TaskEnterType == ETaskEnterType::None || InTask->IsCompleted()) return;
	if (InTask->TaskState == ETaskState::None)
	{
		if (InTask->ParentTask)
		{
			if (!InTask->ParentTask->IsEntered()) return;
			if (InTask->ParentTask->bSequentialSubTasks)
			{
				for (UTaskBase* Sibling : InTask->ParentTask->SubTasks)
				{
					if (Sibling == InTask) break;
					if (!Sibling->IsLeaved() || !Sibling->IsSucceed()) return;
				}
			}
		}
		FString FailedInfo;
		if (!InTask->ArePrerequisitesMet() || !InTask->CheckTaskCondition(FailedInfo)) return;
		InTask->OnEnter();
		if (bSetAsCurrent && !InTask->IsLeaved()) SetCurrentTask(InTask);
	}
	if (InTask->IsEntered() && InTask->HasSubTask(false))
	{
		const TArray<UTaskBase*> Children = InTask->SubTasks;
		for (UTaskBase* Child : Children)
		{
			if (Child && Child->TaskEnterType == ETaskEnterType::Automatic) EnterTask(Child, bSetAsCurrent);
		}
	}
	RefreshTaskMarkers();
}

void UTaskModule::EnterTaskByGUID(const FString& InTaskGUID, bool bSetAsCurrent)
{
	EnterTask(GetTaskByGUID(InTaskGUID), bSetAsCurrent);
}

void UTaskModule::RefreshTask(UTaskBase* InTask)
{
	InTask = ResolveRuntimeTask(InTask);
	if (InTask && InTask->IsEntered())
	{
		if (InTask->IsExecuting()) InTask->OnRefresh();
		else
		{
			const TArray<UTaskBase*> Children = InTask->SubTasks;
			for (UTaskBase* Child : Children) RefreshTask(Child);
		}
	}
}

void UTaskModule::RefreshTaskByGUID(const FString& InTaskGUID)
{
	RefreshTask(GetTaskByGUID(InTaskGUID));
}

void UTaskModule::GuideTask(UTaskBase* InTask)
{
	InTask = ResolveRuntimeTask(InTask);
	if(InTask && InTask->IsEntered())
	{
		InTask->OnGuide();
	}
}

void UTaskModule::GuideTaskByGUID(const FString& InTaskGUID)
{
	GuideTask(GetTaskByGUID(InTaskGUID));
}

void UTaskModule::ExecuteTask(UTaskBase* InTask)
{
	InTask = ResolveRuntimeTask(InTask);
	if(InTask && InTask->GetTaskState() == ETaskState::Entered)
	{
		InTask->OnExecute();
		RefreshTaskMarkers();
	}
}

void UTaskModule::ExecuteTaskByGUID(const FString& InTaskGUID)
{
	ExecuteTask(GetTaskByGUID(InTaskGUID));
}

void UTaskModule::CompleteTask(UTaskBase* InTask, ETaskExecuteResult InTaskExecuteResult)
{
	InTask = ResolveRuntimeTask(InTask);
	if(!InTask || InTaskExecuteResult == ETaskExecuteResult::None) return;

	if(InTaskExecuteResult == ETaskExecuteResult::Skipped)
	{
		FString SkipInfo;
		if(!InTask->CheckTaskSkipAble(SkipInfo))
		{
			return;
		}
	}
	if(InTask->IsEntered() && !InTask->IsCompleted())
	{
		InTask->OnComplete(InTaskExecuteResult);
		if(InTask->ParentTask && InTask->ParentTask->HasSubTask() && InTask->ParentTask->IsAllSubCompleted())
		{
			CompleteTask(InTask->ParentTask, InTask->ParentTask->IsAllSubSucceed() ? ETaskExecuteResult::Succeed : ETaskExecuteResult::Failed);
		}
		RefreshTaskMarkers();
	}
}

void UTaskModule::CompleteTaskByGUID(const FString& InTaskGUID, ETaskExecuteResult InTaskExecuteResult)
{
	CompleteTask(GetTaskByGUID(InTaskGUID), InTaskExecuteResult);
}

void UTaskModule::LeaveTask(UTaskBase* InTask)
{
	InTask = ResolveRuntimeTask(InTask);
	if(!InTask) return;
	
	if(!InTask->IsLeaved())
	{
		if (InTask->bRequireExplicitTurnIn && InTask->TaskExecuteResult != ETaskExecuteResult::Failed && TurningInTask != InTask) return;
		InTask->OnLeave();
		if(InTask->IsCurrent())
		{
			SetCurrentTask(nullptr);
		}
		if(InTask->ParentTask && InTask->ParentTask->IsCompleted() && InTask->ParentTask->HasSubTask() && InTask->ParentTask->IsAllSubLeaved())
		{
			LeaveTask(InTask->ParentTask);
		}
		RefreshTaskMarkers();
	}
}

void UTaskModule::LeaveTaskByGUID(const FString& InTaskGUID)
{
	LeaveTask(GetTaskByGUID(InTaskGUID));
}

bool UTaskModule::IsAllTaskCompleted() const
{
	for(auto Iter1 : Assets)
	{
		for(auto Iter2 : Iter1->RootTasks)
		{
			if(!Iter2->IsCompleted(true))
			{
				return false;
			}
		}
	}
	return true;
}

void UTaskModule::SetCurrentTask(UTaskBase* InTask)
{
	InTask = ResolveRuntimeTask(InTask);
	if (CurrentTask == InTask) return;
	CurrentTask = InTask;
	RefreshTaskMarkers();
	UEventModuleStatics::BroadcastEvent(UEventHandle_CurrentTaskChanged::StaticClass(), this, {CurrentTask});
}

UTaskBase* UTaskModule::GetGuidanceTask() const
{
	TFunction<UTaskBase*(UTaskBase*)> Resolve = [&](UTaskBase* Task) -> UTaskBase*
	{
		if(!Task || Task->IsLeaved()) return nullptr;
		for(UTaskBase* Child : Task->SubTasks)
		{
			if(Child && Child->bRuntimeSelected)
			{
				if(UTaskBase* Result = Resolve(Child)) return Result;
			}
		}
		for(UTaskBase* Child : Task->SubTasks)
		{
			if(UTaskBase* Result = Resolve(Child)) return Result;
		}
		const ETaskStage Stage = Task->GetTaskStage();
		return Task->Marker.bVisible && Task->Target.IsValid() && (Stage == ETaskStage::Active || Stage == ETaskStage::Deliverable) ? Task : nullptr;
	};
	return Resolve(CurrentTask);
}

bool UTaskModule::IsExistTaskByGUID(const FString& InTaskGUID) const
{
	return GetTaskByGUID(InTaskGUID) != nullptr;
}

UTaskBase* UTaskModule::GetTaskByGUID(const FString& InTaskGUID) const
{
	UTaskBase* Result = nullptr;
	for (UTaskAsset* Asset : Assets)
	{
		if (UTaskBase* Task = Asset->TaskMap.FindRef(InTaskGUID))
		{
			if (Result) return nullptr;
			Result = Task;
		}
	}
	return Result;
}

UTaskBase* UTaskModule::ResolveTask(const FTaskReference& Reference, UTaskAsset* InContext) const
{
	FGuid InstanceID = Reference.InstanceID;
	if(!InstanceID.IsValid() && InContext)
	{
		const UObject* ContextSource = InContext->SourceObject ? InContext->SourceObject : InContext;
		if(FSoftObjectPath(ContextSource) == Reference.Asset.ToSoftObjectPath()) InstanceID = InContext->InstanceID;
	}
	for (UTaskAsset* Asset : Assets)
	{
		if (Asset->InstanceID == InstanceID && (Asset == Reference.Asset.Get() || (Asset->SourceObject && FSoftObjectPath(Asset->SourceObject) == Reference.Asset.ToSoftObjectPath())))
		{
			return Asset->TaskMap.FindRef(Reference.TaskGUID);
		}
	}
	return nullptr;
}

UTaskBase* UTaskModule::EnsureTask(const FTaskReference& Reference)
{
	if (UTaskBase* Task = ResolveTask(Reference)) return Task;
	if (UTaskAsset* Asset = Reference.Asset.LoadSynchronous()) AddAssetInternal(Asset, Reference.InstanceID, FGuid());
	return ResolveTask(Reference);
}

bool UTaskModule::TurnInTask(UTaskBase* InTask, AActor* InTarget)
{
	InTask = ResolveRuntimeTask(InTask);
	if (!InTask || InTask->TaskState != ETaskState::Completed || InTask->TaskExecuteResult != ETaskExecuteResult::Succeed) return false;
	UTaskAsset* Asset = InTask->GetTaskAsset();
	if (InTask->bRequireExplicitTurnIn)
	{
		if(!InTarget || !Asset || !Asset->AgentID.IsValid() || !InTarget->Implements<USceneActorInterface>() || ISceneActorInterface::Execute_GetActorID(InTarget) != Asset->AgentID) return false;
		FText Reason;
		if(!InTask->CanTurnIn(InTarget, Reason) || !InTask->CommitTurnIn(InTarget)) return false;
	}
	TGuardValue<UTaskBase*> Guard(TurningInTask, InTask);
	LeaveTask(InTask);
	return InTask->TaskState == ETaskState::Leaved;
}

void UTaskModule::ReportTaskEvent(FGameplayTag InEventTag, FGameplayTag InTargetTag, int32 InCount, FPrimaryAssetId InTargetAssetID, FName InTargetName)
{
	if (!InEventTag.IsValid() || InCount <= 0 || bLoadingTasks || ModuleState == EModuleState::Paused) return;
	TArray<UTaskBase*> ExecutingTasks;
	for (UTaskAsset* Asset : Assets)
	{
		for (const auto& Pair : Asset->TaskMap)
		{
			if (Pair.Value->IsExecuting() && !Pair.Value->Objectives.IsEmpty()) ExecutingTasks.Add(Pair.Value);
		}
	}
	for (UTaskBase* Task : ExecutingTasks)
	{
		if (ResolveRuntimeTask(Task)) Task->ApplyObjectiveEvent(InEventTag, InTargetTag, InCount, InTargetAssetID, InTargetName);
	}
	RefreshTaskMarkers();
}

void UTaskModule::RemoveTaskMarkers(const UTaskAsset* InAsset)
{
	if(!USceneModule::IsValid())
	{
		if(!InAsset) TaskMarkerIDs.Reset();
		return;
	}
	TSet<FGuid> Removed;
	if(InAsset)
	{
		for(const auto& Pair : InAsset->TaskMap) Removed.Add(MakeTaskMarkerID(InAsset, Pair.Key));
	}
	else
	{
		Removed = TaskMarkerIDs;
	}
	for(const FGuid& MarkerID : Removed)
	{
		USceneModule::Get().RemoveMarker(MarkerID);
		TaskMarkerIDs.Remove(MarkerID);
	}
}

void UTaskModule::RefreshTaskMarkers()
{
	if(bLoadingTasks) return;
	if(!USceneModule::IsValid())
	{
		OnTaskAssetsChanged.Broadcast();
		return;
	}
	USceneModule& Scene = USceneModule::Get();
	TSet<FGuid> DesiredIDs;
	FGuid CurrentMarkerID;
	UTaskBase* GuidanceTask = GetGuidanceTask();
	for(UTaskAsset* Asset : Assets)
	{
		if(!Asset) continue;
		for(const auto& Pair : Asset->TaskMap)
		{
			UTaskBase* Task = Pair.Value;
			if(!Task || !Task->Marker.bVisible) continue;
			const ETaskStage Stage = Task->GetTaskStage();
			if(Stage != ETaskStage::Active && Stage != ETaskStage::Deliverable) continue;
			FTaskTarget Target = Task->Target;
			if(Stage == ETaskStage::Deliverable && Task->bRequireExplicitTurnIn && Asset->AgentID.IsValid())
			{
				Target.ActorID = Asset->AgentID;
				Target.AreaName = NAME_None;
				Target.Location = Asset->AgentLocation;
			}
			if(!Target.IsValid()) continue;
			FSceneMarker Marker;
			Marker.MarkerID = MakeTaskMarkerID(Asset, Pair.Key);
			Marker.MarkerTag = Stage == ETaskStage::Deliverable ? TaskTags::Marker_Deliverable : TaskTags::Marker_Active;
			Marker.DisplayName = Task->TaskDisplayName;
			Marker.Icon = Task->Marker.Icon;
			Marker.Color = Task->Marker.Color;
			Marker.Location = Target.Location;
			Marker.Offset = Target.Offset;
			Marker.ActorID = Target.ActorID;
			Marker.AreaName = Target.AreaName;
			Marker.Priority = Task->Marker.Priority;
			Marker.Channels = Task->Marker.Channels;
			if(Task->Marker.bOnlyShowNavigationWhenTracked && GuidanceTask != Task)
			{
				Marker.Channels &= ~static_cast<int32>(ESceneMarkerChannel::MiniMap | ESceneMarkerChannel::Compass | ESceneMarkerChannel::World);
			}
			if(GuidanceTask == Task)
			{
				CurrentMarkerID = Marker.MarkerID;
			}
			DesiredIDs.Add(Marker.MarkerID);
			if(!Scene.UpdateMarker(Marker)) Scene.AddMarker(Marker);
		}
	}
	const TSet<FGuid> PreviousIDs = TaskMarkerIDs;
	for(const FGuid& MarkerID : PreviousIDs)
	{
		if(!DesiredIDs.Contains(MarkerID)) Scene.RemoveMarker(MarkerID);
	}
	TaskMarkerIDs = MoveTemp(DesiredIDs);
	Scene.SetTrackedMarker(CurrentMarkerID);
	OnTaskAssetsChanged.Broadcast();
}
