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

	Assets = TArray<UTaskAsset*>();
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
		AddAsset(DefaultAsset);
	}
}

void UTaskModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		SwitchAsset(DefaultAsset);
	}
}

void UTaskModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);

	if(bInEditor) return;

	if(CurrentTask)
	{
		CurrentTask->Refresh();
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

void UTaskModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FTaskModuleSaveData>();

	if(PHASEC(InPhase, EPhase::Primary))
	{
		if(SaveData.IsSaved())
		{
			Assets = SaveData.Assets;
			CurrentAsset = SaveData.CurrentAsset;
			CurrentTask = SaveData.CurrentTask;
			for(auto Iter1 : Assets)
			{
				for(auto Iter2 : Iter1->TaskMap)
				{
					if(SaveData.TaskDataMap.Contains(Iter2.Key))
					{
						Iter2.Value->LoadSaveData(&SaveData.TaskDataMap[Iter2.Key]);
					}
				}
			}
		}
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
		CurrentAsset = nullptr;
		Assets.Empty();
	}
}

FSaveData* UTaskModule::ToData()
{
	static FTaskModuleSaveData SaveData;
	SaveData = FTaskModuleSaveData();
	
	SaveData.Assets = Assets;
	SaveData.CurrentAsset = CurrentAsset;
	SaveData.CurrentTask = CurrentTask;
	for(auto Iter1 : Assets)
	{
		for(auto Iter2 : Iter1->TaskMap)
		{
			SaveData.TaskDataMap.Add(Iter2.Key, Iter2.Value->GetSaveDataRef<FSaveData>(true));
		}
	}

	return &SaveData;
}

FString UTaskModule::GetModuleDebugMessage()
{
	return FString::Printf(TEXT("CurrentTask: %s"), CurrentTask ? *CurrentTask->TaskDisplayName.ToString() : TEXT("None"));
}

UTaskAsset* UTaskModule::GetAsset(UTaskAsset* InAsset) const
{
	for(auto Iter : Assets)
	{
		if(Iter->SourceObject == InAsset)
		{
			return Iter;
		}
	}
	return nullptr;
}

void UTaskModule::AddAsset(UTaskAsset* InAsset)
{
	if(!GetAsset(InAsset))
	{
		InAsset = InAsset->Duplicate<UTaskAsset>();
		Assets.Add(InAsset);
		InAsset->Initialize();
	}
}

void UTaskModule::RemoveAsset(UTaskAsset* InAsset)
{
	if(UTaskAsset* Asset = GetAsset(InAsset))
	{
		Assets.Remove(Asset);
	}
}

void UTaskModule::SwitchAsset(UTaskAsset* InAsset)
{
	if(InAsset && !InAsset->SourceObject) InAsset = GetAsset(InAsset);
	
	if(!InAsset || !Assets.Contains(InAsset) || CurrentAsset == InAsset) return;

	CurrentAsset = InAsset;

	WHDebug(FString::Printf(TEXT("切换任务源: %s"), !CurrentAsset->DisplayName.IsEmpty() ? *CurrentAsset->DisplayName.ToString() : *CurrentAsset->GetName()), EDM_All, EDC_Task, EDV_Log, FColor::Green, 5.f);

	if(InAsset->bAutoEnterFirst)
	{
		EnterTask(GetFirstTask());
	}
}

void UTaskModule::RestoreTask(UTaskBase* InTask)
{
	if(InTask && InTask->GetTaskState() != ETaskState::None)
	{
		InTask->OnRestore();
	}
}

void UTaskModule::RestoreTaskByGUID(const FString& InTaskGUID)
{
	RestoreTask(GetTaskByGUID(InTaskGUID));
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
		EnterTask(InTask->ParentTask);
	}
	
	if((!InTask->ParentTask || InTask->ParentTask->IsEntered()) && !InTask->IsEntered())
	{
		if(CurrentTask && !CurrentTask->IsParentOf(InTask))
		{
			CurrentTask->Leave();
		}
		UTaskBase* LastTask = CurrentTask;
		CurrentTask = InTask;
		InTask->OnEnter(LastTask);
		if(InTask->HasSubTask(false))
		{
			if(InTask->SubTasks.IsValidIndex(0))
			{
				if(UTaskBase* SubTask = InTask->SubTasks[0]; SubTask->TaskEnterType == ETaskEnterType::Automatic)
				{
					SubTask->Enter();
				}
			}
		}
	}
}

void UTaskModule::EnterTaskByGUID(const FString& InTaskGUID)
{
	EnterTask(GetTaskByGUID(InTaskGUID));
}

void UTaskModule::RefreshTask(UTaskBase* InTask)
{
	if(InTask && InTask->IsExecuting())
	{
		InTask->OnRefresh();
	}
}

void UTaskModule::RefreshTaskByGUID(const FString& InTaskGUID)
{
	RefreshTask(GetTaskByGUID(InTaskGUID));
}

void UTaskModule::GuideTask(UTaskBase* InTask)
{
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
	if(!InTask) return;

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
			CompleteTask(InTask->ParentTask, InTask->ParentTask->IsAllSubExecuteSucceed() ? ETaskExecuteResult::Succeed : ETaskExecuteResult::Failed);
		}
	}
}

void UTaskModule::CompleteTaskByGUID(const FString& InTaskGUID, ETaskExecuteResult InTaskExecuteResult)
{
	CompleteTask(GetTaskByGUID(InTaskGUID), InTaskExecuteResult);
}

void UTaskModule::LeaveTask(UTaskBase* InTask)
{
	if(!InTask) return;
	
	if(!InTask->IsLeaved())
	{
		if(InTask == CurrentTask)
		{
			if(InTask->ParentTask)
			{
				if(InTask->ParentTask->HasSubTask(false))
				{
					const int32 Index = InTask->TaskIndex + 1;
					if(InTask->ParentTask->SubTasks.IsValidIndex(Index))
					{
						if(UTaskBase* SubTask = InTask->ParentTask->SubTasks[Index]; SubTask->TaskEnterType == ETaskEnterType::Automatic)
						{
							SubTask->Enter();
						}
					}
				}
			}
			else
			{
				if(InTask->IsRootTask())
				{
					const int32 Index = InTask->TaskIndex + 1;
					if(GetRootTasks().IsValidIndex(Index))
					{
						if(UTaskBase* RootTask = GetRootTasks()[Index]; RootTask->TaskEnterType == ETaskEnterType::Automatic)
						{
							EnterTask(RootTask);
						}
					}
				}
			}
			if(CurrentTask == InTask)
			{
				CurrentTask = nullptr;
			}
		}
		InTask->OnLeave();
	}
}

void UTaskModule::LeaveTaskByGUID(const FString& InTaskGUID)
{
	LeaveTask(GetTaskByGUID(InTaskGUID));
}

bool UTaskModule::IsAllTaskCompleted() const
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

UTaskBase* UTaskModule::GetCurrentRootTask() const
{
	if(CurrentTask)
	{
		return CurrentTask->IsRootTask() ? CurrentTask : CurrentTask->RootTask;
	}
	return nullptr;
}
