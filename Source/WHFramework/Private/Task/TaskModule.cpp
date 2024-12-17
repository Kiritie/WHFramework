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

	for(auto Iter1 : Assets)
	{
		for(auto Iter2 : Iter1->RootTasks)
		{
			if(Iter2->TaskEnterType == ETaskEnterType::Automatic)
			{
				Iter2->Enter(!CurrentTask);
			}
			Iter2->Refresh();
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

void UTaskModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FTaskModuleSaveData>();

	if(PHASEC(InPhase, EPhase::Primary))
	{
		if(SaveData.IsSaved())
		{
			Assets = SaveData.Assets;
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
		for(auto Iter1 : Assets)
		{
			for(auto Iter2 : Iter1->RootTasks)
			{
				Iter2->Restore();
			}
		}
		CurrentTask = nullptr;
		Assets.Empty();
	}
}

FSaveData* UTaskModule::ToData()
{
	static FTaskModuleSaveData SaveData;
	SaveData = FTaskModuleSaveData();
	
	SaveData.Assets = Assets;
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

void UTaskModule::EnterTask(UTaskBase* InTask, bool bSetAsCurrent)
{
	if(!InTask || InTask->TaskEnterType == ETaskEnterType::None) return;

	FString FailedStr;
	if(!InTask->CheckTaskCondition(FailedStr))
	{
		return;
	}
	
	if(!InTask->IsEntered())
	{
		InTask->OnEnter();
	}

	if(InTask->HasSubTask(false))
	{
		for(auto Iter : InTask->SubTasks)
		{
			if(Iter->TaskEnterType == ETaskEnterType::Automatic)
			{
				Iter->Enter(bSetAsCurrent);
			}
		}
	}

	if(bSetAsCurrent && !InTask->IsRootTask())
	{
		SetCurrentTask(InTask);
	}
}

void UTaskModule::EnterTaskByGUID(const FString& InTaskGUID, bool bSetAsCurrent)
{
	EnterTask(GetTaskByGUID(InTaskGUID), bSetAsCurrent);
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
		InTask->OnLeave();
		if(InTask->IsCurrent())
		{
			SetCurrentTask(nullptr);
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
	CurrentTask = InTask;
	UEventModuleStatics::BroadcastEvent(UEventHandle_CurrentTaskChanged::StaticClass(), this, {CurrentTask});
}

UTaskBase* UTaskModule::GetTaskByGUID(const FString& InTaskGUID) const
{
	for(auto Iter1 : Assets)
	{
		return *Iter1->TaskMap.Find(InTaskGUID);
	}
	return nullptr;
}
