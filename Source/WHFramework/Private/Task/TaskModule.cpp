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

	DefaultAsset = nullptr;
	CurrentAsset = nullptr;
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

	if(DefaultAsset)
	{
		SetCurrentAsset(DefaultAsset);
	}
}

void UTaskModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		if(bAutoEnterFirst && !CurrentTask)
		{
			EnterTask(GetFirstTask());
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
				else if(CurrentTask->ParentTask->IsRootTask() && GetRootTasks().IsValidIndex(CurrentTask->ParentTask->TaskIndex + 1))
				{
					EnterTask(GetRootTasks()[CurrentTask->ParentTask->TaskIndex + 1]);
				}
			}
		}
		else
		{
			if(CurrentTask->GetTaskState() != ETaskState::Leaved)
			{
				CurrentTask->Refresh();
			}
			else if(GetRootTasks().IsValidIndex(CurrentTask->ParentTask->TaskIndex + 1))
			{
				EnterTask(GetRootTasks()[CurrentTask->ParentTask->TaskIndex + 1]);
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
		if(GetTaskMap().Contains(Iter.Key))
		{
			GetTaskMap()[Iter.Key]->LoadSaveData(&Iter.Value);
		}
	}
	if(!CurrentTask)
	{
		EnterTask(GetFirstTask());
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
		for(auto Iter : GetRootTasks())
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

	for(auto Iter : GetTaskMap())
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

bool UTaskModule::IsAllTaskCompleted()
{
	for(auto Iter : GetRootTasks())
	{
		if(!Iter->IsCompleted(true))
		{
			return false;
		}
	}
	return true;
}

void UTaskModule::SetCurrentAsset(UTaskAsset* InTaskAsset, bool bInAutoEnterFirst)
{
	if(!InTaskAsset || (CurrentAsset && InTaskAsset == CurrentAsset->SourceObject)) return;

	CurrentAsset = DuplicateObject<UTaskAsset>(InTaskAsset, this);
	CurrentAsset->Initialize(InTaskAsset);

	WHDebug(FString::Printf(TEXT("切换任务源: %s"), !CurrentAsset->DisplayName.IsEmpty() ? *CurrentAsset->DisplayName.ToString() : *CurrentAsset->GetName()), EDM_All, EDC_Procedure, EDV_Log, FColor::Green, 5.f);

	if(bInAutoEnterFirst)
	{
		EnterTask(GetFirstTask());
	}
}

UTaskBase* UTaskModule::GetCurrentRootTask() const
{
	if(CurrentTask)
	{
		return CurrentTask->IsRootTask() ? CurrentTask : CurrentTask->RootTask;
	}
	return nullptr;
}
