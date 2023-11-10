// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/TaskModule.h"

#include "Main/MainModule.h"
#include "Task/Base/TaskBase.h"
#include "Character/CharacterModuleTypes.h"
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

	bAutoEnterFirst = false;

	RootTasks = TArray<UTaskBase*>();

	FirstTask = nullptr;
	CurrentTask = nullptr;
	TaskMap = TMap<FString, UTaskBase*>();
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

	ClearAllTask();
}
#endif

void UTaskModule::OnInitialize()
{
	Super::OnInitialize();
}

void UTaskModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		if(!FirstTask && RootTasks.Num() > 0)
		{
			FirstTask = RootTasks[0];
		}
		for(auto Iter : RootTasks)
		{
			if(Iter)
			{
				Iter->OnInitialize();
			}
		}
	}
	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bModuleAutoSave)
		{
			Load();
		}
	}
	if(PHASEC(InPhase, EPhase::Final))
	{
		if(bAutoEnterFirst && !CurrentTask)
		{
			EnterTask(FirstTask);
		}
	}
}

void UTaskModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	if(CurrentTask)
	{
		if(!CurrentTask->IsRootTask())
		{
			if(CurrentTask->GetTaskState() != ETaskState::Leaved)
			{
				CurrentTask->Refresh();
			}
			else if(CurrentTask->ParentTask)
			{
				if(CurrentTask->ParentTask->GetTaskState() != ETaskState::Leaved)
				{
					CurrentTask->ParentTask->Refresh();
				}
				else if(CurrentTask->ParentTask->IsRootTask() && RootTasks.IsValidIndex(CurrentTask->ParentTask->TaskIndex + 1))
				{
					EnterTask(RootTasks[CurrentTask->ParentTask->TaskIndex + 1]);
				}
			}
		}
		else
		{
			if(CurrentTask->GetTaskState() != ETaskState::Leaved)
			{
				CurrentTask->Refresh();
			}
			else if(RootTasks.IsValidIndex(CurrentTask->ParentTask->TaskIndex + 1))
			{
				EnterTask(RootTasks[CurrentTask->ParentTask->TaskIndex + 1]);
			}
		}
	}
}

void UTaskModule::OnPause()
{
	Super::OnPause();
}

void UTaskModule::OnUnPause()
{
	Super::OnUnPause();
}

void UTaskModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);

	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bModuleAutoSave)
		{
			Save();
		}
	}
}

void UTaskModule::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.ArIsSaveGame)
	{
		if(Ar.IsLoading())
		{
			Ar << CurrentTask;
		}
		else if(Ar.IsSaving())
		{
			Ar << CurrentTask;
		}
	}
}

void UTaskModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	const auto& SaveData = InSaveData->CastRef<FTaskModuleSaveData>();

	for(auto Iter : SaveData.TaskItemMap)
	{
		if(TaskMap.Contains(Iter.Key))
		{
			TaskMap[Iter.Key]->LoadSaveData(&Iter.Value);
		}
	}
	if(!CurrentTask)
	{
		EnterTask(FirstTask);
	}
}

void UTaskModule::UnloadData(EPhase InPhase)
{
	if(PHASEC(InPhase, EPhase::Primary))
	{
		if(CurrentTask)
		{
			CurrentTask->Leave();
		}
		for(auto Iter : RootTasks)
		{
			Iter->Restore();
		}
		CurrentTask = nullptr;
	}
}

FSaveData* UTaskModule::ToData()
{
	static FTaskModuleSaveData SaveData;
	SaveData = FTaskModuleSaveData();

	for(auto Iter : TaskMap)
	{
		SaveData.TaskItemMap.Add(Iter.Key, Iter.Value->GetSaveDataRef<FSaveData>(true));
	}
	return &SaveData;
}

void UTaskModule::RestoreTask(UTaskBase* InTask)
{
	if(InTask && InTask->GetTaskState() != ETaskState::None)
	{
		InTask->OnRestore();
	}
}

void UTaskModule::EnterTask(UTaskBase* InTask)
{
	if(!InTask || InTask->TaskEnterType == ETaskEnterType::None) return;

	FString FailedStr;
	if(!InTask->CheckTaskCondition(FailedStr))
	{
		return;
	}
	
	if(InTask->ParentTask && !InTask->ParentTask->IsEntered())
	{
		InTask->ParentTask->CurrentSubTaskIndex = InTask->TaskIndex - 1;
		EnterTask(InTask->ParentTask);
		return;
	}
	if(InTask && InTask->GetTaskState() == ETaskState::None)
	{
		if(CurrentTask && !CurrentTask->IsParentOf(InTask))
		{
			CurrentTask->Leave();
		}
		UTaskBase* LastTask = CurrentTask;
		CurrentTask = InTask;
		InTask->OnEnter(LastTask);
	}
}

void UTaskModule::RefreshTask(UTaskBase* InTask)
{
	if(InTask && InTask->GetTaskState() == ETaskState::Executing)
	{
		InTask->OnRefresh();
	}
}

void UTaskModule::GuideTask(UTaskBase* InTask)
{
	if(InTask && InTask->IsEntered())
	{
		InTask->OnGuide();
	}
}

void UTaskModule::ExecuteTask(UTaskBase* InTask)
{
	if(InTask && InTask->GetTaskState() == ETaskState::Entered)
	{
		InTask->OnExecute();
	}
}

void UTaskModule::CompleteTask(UTaskBase* InTask, ETaskExecuteResult InTaskExecuteResult)
{
	if(!InTask) return;

	if(InTaskExecuteResult == ETaskExecuteResult::Skipped)
	{
		FString SkipInfo;
		if(!InTask->CheckTaskSkipAble(SkipInfo))
		{
			return;
		}
	}
	if(!InTask->IsCompleted())
	{
		InTask->OnComplete(InTaskExecuteResult);
	}
	for(auto Iter : InTask->SubTasks)
	{
		if(Iter) Iter->Complete(InTaskExecuteResult);
	}
}

void UTaskModule::LeaveTask(UTaskBase* InTask)
{
	if(!InTask) return;
	
	if(InTask->GetTaskState() != ETaskState::Completed)
	{
		InTask->Complete(ETaskExecuteResult::Skipped);
	}
	if(InTask->GetTaskState() != ETaskState::Leaved)
	{
		InTask->OnLeave();
	}
}

void UTaskModule::ClearAllTask()
{
	for(auto Iter : RootTasks)
	{
		if(Iter)
		{
#if WITH_EDITOR
			Iter->OnUnGenerate();
#else
			Iter->ConditionalBeginDestroy();
#endif
		}
	}
	
	RootTasks.Empty();

	TaskMap.Empty();

	Modify();
}

bool UTaskModule::IsAllTaskCompleted()
{
	for(auto Iter : RootTasks)
	{
		if(!Iter->IsCompleted(true))
		{
			return false;
		}
	}
	return true;
}

UTaskBase* UTaskModule::GetCurrentRootTask() const
{
	if(CurrentTask)
	{
		return CurrentTask->IsRootTask() ? CurrentTask : CurrentTask->RootTask;
	}
	return nullptr;
}

#if WITH_EDITOR
void UTaskModule::GenerateListItem(TArray<TSharedPtr<FTaskListItem>>& OutTaskListItems)
{
	OutTaskListItems = TArray<TSharedPtr<FTaskListItem>>();
	for (int32 i = 0; i < RootTasks.Num(); i++)
	{
		auto Item = MakeShared<FTaskListItem>();
		RootTasks[i]->GenerateListItem(Item);
		OutTaskListItems.Add(Item);
	}
}

void UTaskModule::UpdateListItem(TArray<TSharedPtr<FTaskListItem>>& OutTaskListItems)
{
	for (int32 i = 0; i < RootTasks.Num(); i++)
	{
		RootTasks[i]->TaskIndex = i;
		RootTasks[i]->TaskHierarchy = 0;
		RootTasks[i]->UpdateListItem(OutTaskListItems[i]);
	}
}
#endif
