// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/TaskModule.h"

#include "Main/MainModule.h"
#include "Task/Base/TaskBase.h"
#include "Character/CharacterModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Task/EventHandle_CurrentTaskChanged.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/TaskSaveGame.h"
#include "Task/TaskModuleNetworkComponent.h"

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
			if (Task->TaskEnterType == ETaskEnterType::Automatic)
				EnterTask(Task, !CurrentTask);
			if (Task->IsEntered()) EnterTask(Task, false);
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
	return (Source ? Source->GetPathName() : FString()) + TEXT(":") + GUID;
}

void UTaskModule::ClearRuntimeAssets()
{
	for (UTaskAsset* Asset : Assets)
		if (Asset) for (const auto& Pair : Asset->TaskMap)
			if (Pair.Value) Pair.Value->OnSuspend();
	SetCurrentTask(nullptr);
	Assets.Reset();
	PendingResume.Reset();
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
		for (const auto& Path : Data.AssetPaths)
		{
			if (UTaskAsset* Source = Path.LoadSynchronous())
			{
				AddAsset(Source);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Missing task asset in save: %s"), *Path.ToString());
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
		CurrentTask = nullptr;
		for (UTaskAsset* Asset : Assets)
		{
			if (Asset->SourceObject && Asset->SourceObject->GetPathName() == Data.CurrentTaskAssetPath)
			{
				CurrentTask = Asset->TaskMap.FindRef(Data.CurrentTaskGUID);
			}
		}
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
		OnTaskAssetsChanged.Broadcast();
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
		CachedSaveData.AssetPaths.Add(Cast<UTaskAsset>(Asset->SourceObject));
		for (const auto& Pair : Asset->TaskMap)
			if (Pair.Value) CachedSaveData.TaskRecords.Add(GetSaveKey(Asset, Pair.Key), Pair.Value->CaptureRuntimeData());
	}
	if (CurrentTask && CurrentTask->GetTaskAsset())
	{
		CachedSaveData.CurrentTaskGUID = CurrentTask->TaskGUID;
		CachedSaveData.CurrentTaskAssetPath = CurrentTask->GetTaskAsset()->SourceObject->GetPathName();
	}
	return &CachedSaveData;
}

FString UTaskModule::GetModuleDebugMessage()
{
	return FString::Printf(TEXT("CurrentTask: %s"), CurrentTask ? *CurrentTask->TaskDisplayName.ToString() : TEXT("None"));
}

UTaskAsset* UTaskModule::GetAsset(UTaskAsset* InAsset) const
{
	if (!InAsset) return nullptr;
	for (UTaskAsset* Asset : Assets)
		if (Asset && (Asset == InAsset || Asset->SourceObject == InAsset)) return Asset;
	return nullptr;
}

UTaskBase* UTaskModule::ResolveRuntimeTask(UTaskBase* Task) const
{
	if (!Task) return nullptr;
	if (UTaskAsset* Asset = GetAsset(Task->GetTaskAsset())) return Asset->TaskMap.FindRef(Task->TaskGUID);
	return nullptr;
}

void UTaskModule::AddAsset(UTaskAsset* InAsset)
{
	if (!InAsset || GetAsset(InAsset)) return;
	TArray<FText> Errors;
	if (!InAsset->ValidateTasks(Errors)) return;
	UTaskAsset* RuntimeAsset = DuplicateObject<UTaskAsset>(InAsset, this);
	RuntimeAsset->SourceObject = InAsset;
	Assets.Add(RuntimeAsset);
	RuntimeAsset->Initialize();
	if (!bLoadingTasks) OnTaskAssetsChanged.Broadcast();
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
		if (!bLoadingTasks) OnTaskAssetsChanged.Broadcast();
	}
}

void UTaskModule::RestoreTask(UTaskBase* InTask)
{
	InTask = ResolveRuntimeTask(InTask);
	if(InTask && InTask->GetTaskState() != ETaskState::None)
	{
		if (CurrentTask == InTask || (CurrentTask && InTask->IsParentOf(CurrentTask))) SetCurrentTask(nullptr);
		InTask->OnRestore();
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
					if (!Sibling->IsCompleted() || !Sibling->IsSucceed()) return;
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
		if(InTask->ParentTask && InTask->ParentTask->HasSubTask() && InTask->ParentTask->IsAllSubLeaved())
		{
			LeaveTask(InTask->ParentTask);
		}
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
	UEventModuleStatics::BroadcastEvent(UEventHandle_CurrentTaskChanged::StaticClass(), this, {CurrentTask});
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

UTaskBase* UTaskModule::ResolveTask(const FTaskReference& Reference) const
{
	for (UTaskAsset* Asset : Assets)
	{
		if (Asset == Reference.Asset.Get() || (Asset->SourceObject && FSoftObjectPath(Asset->SourceObject) == Reference.Asset.ToSoftObjectPath()))
		{
			return Asset->TaskMap.FindRef(Reference.TaskGUID);
		}
	}
	return nullptr;
}

UTaskBase* UTaskModule::EnsureTask(const FTaskReference& Reference)
{
	if (UTaskBase* Task = ResolveTask(Reference)) return Task;
	if (UTaskAsset* Asset = Reference.Asset.LoadSynchronous()) AddAsset(Asset);
	return ResolveTask(Reference);
}

bool UTaskModule::TurnInTask(UTaskBase* InTask, AActor* InTarget)
{
	InTask = ResolveRuntimeTask(InTask);
	if (!InTask || InTask->TaskState != ETaskState::Completed || InTask->TaskExecuteResult != ETaskExecuteResult::Succeed) return false;
	if (InTask->bRequireExplicitTurnIn && (!InTarget || (!InTask->TurnInActorTag.IsNone() && !InTarget->ActorHasTag(InTask->TurnInActorTag)))) return false;
	TGuardValue<UTaskBase*> Guard(TurningInTask, InTask);
	LeaveTask(InTask);
	return InTask->TaskState == ETaskState::Leaved;
}

void UTaskModule::ReportTaskEvent(FGameplayTag InEventTag, FGameplayTag InTargetTag, int32 InCount, FPrimaryAssetId InTargetAssetID)
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
		if (ResolveRuntimeTask(Task)) Task->ApplyObjectiveEvent(InEventTag, InTargetTag, InCount, InTargetAssetID);
	}
}
