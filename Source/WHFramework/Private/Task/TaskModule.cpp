// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/TaskModule.h"

#include "Main/MainModule.h"
#include "Engine/TargetPoint.h"
#include "Kismet/GameplayStatics.h"
#include "Task/Base/TaskBase.h"
#include "Task/Base/RootTaskBase.h"
#include "Net/UnrealNetwork.h"
#include "Character/CharacterModuleBPLibrary.h"
#include "Character/CharacterModuleTypes.h"
#include "Event/EventModule.h"
#include "Event/EventModuleBPLibrary.h"
#include "Network/NetworkModuleBPLibrary.h"
#include "Task/TaskModuleBPLibrary.h"
#include "Task/TaskModuleNetworkComponent.h"
#include "Scene/SceneModule.h"
#include "Scene/SceneModuleBPLibrary.h"
		
IMPLEMENTATION_MODULE(ATaskModule)

// ParamSets default values
ATaskModule::ATaskModule()
{
	ModuleName = FName("TaskModule");

	bAutoEnterFirst = false;

	RootTasks = TArray<URootTaskBase*>();

	FirstTask = nullptr;
	CurrentTask = nullptr;
	TaskMap = TMap<FString, UTaskBase*>();
}

ATaskModule::~ATaskModule()
{
	TERMINATION_MODULE(ATaskModule)
}

#if WITH_EDITOR
void ATaskModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void ATaskModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();

	ClearAllTask();
}
#endif

void ATaskModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

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

void ATaskModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	if(InPhase == EPhase::Final && bAutoEnterFirst)
	{
		EnterTask(FirstTask);
	}
}

void ATaskModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	if(CurrentTask)
	{
		if(!CurrentTask->IsA<URootTaskBase>())
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
				else if(CurrentTask->ParentTask->IsA<URootTaskBase>() && RootTasks.IsValidIndex(CurrentTask->ParentTask->TaskIndex + 1))
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

void ATaskModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void ATaskModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void ATaskModule::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();
}

void ATaskModule::RestoreTask(UTaskBase* InTask)
{
	if(InTask && InTask->GetTaskState() != ETaskState::None)
	{
		InTask->OnRestore();
	}
}

void ATaskModule::EnterTask(UTaskBase* InTask)
{
	if(!InTask || InTask->TaskEnterType == ETaskEnterType::None) return;
	
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
		InTask->OnEnter(CurrentTask);

		CurrentTask = InTask;
	}
}

void ATaskModule::RefreshTask(UTaskBase* InTask)
{
	if(InTask && InTask->GetTaskState() == ETaskState::Executing)
	{
		InTask->OnRefresh();
	}
}

void ATaskModule::GuideTask(UTaskBase* InTask)
{
	if(InTask && InTask->IsEntered())
	{
		InTask->OnGuide();
	}
}

void ATaskModule::ExecuteTask(UTaskBase* InTask)
{
	if(InTask && InTask->GetTaskState() == ETaskState::Entered)
	{
		InTask->OnExecute();
	}
}

void ATaskModule::CompleteTask(UTaskBase* InTask, ETaskExecuteResult InTaskExecuteResult)
{
	if(!InTask) return;
	
	if(!InTask->IsCompleted())
	{
		GetWorldTimerManager().SetTimerForNextTick([=]()
		{
			InTask->OnComplete(InTaskExecuteResult);
		});
	}
	for(auto Iter : InTask->SubTasks)
	{
		if(Iter) Iter->Complete(InTaskExecuteResult);
	}
}

void ATaskModule::LeaveTask(UTaskBase* InTask)
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

void ATaskModule::ClearAllTask()
{
	for(auto Iter : RootTasks)
	{
		if(Iter)
		{
			#if(WITH_EDITOR)
			Iter->OnUnGenerate();
			#endif
			Iter->ConditionalBeginDestroy();
		}
	}
	
	RootTasks.Empty();

	Modify();
}

bool ATaskModule::IsAllTaskCompleted()
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

#if WITH_EDITOR
void ATaskModule::GenerateListItem(TArray<TSharedPtr<FTaskListItem>>& OutTaskListItems)
{
	OutTaskListItems = TArray<TSharedPtr<FTaskListItem>>();
	for (int32 i = 0; i < RootTasks.Num(); i++)
	{
		auto Item = MakeShared<FTaskListItem>();
		RootTasks[i]->GenerateListItem(Item);
		OutTaskListItems.Add(Item);
	}
}

void ATaskModule::UpdateListItem(TArray<TSharedPtr<FTaskListItem>>& OutTaskListItems)
{
	for (int32 i = 0; i < RootTasks.Num(); i++)
	{
		RootTasks[i]->TaskIndex = i;
		RootTasks[i]->TaskHierarchy = 0;
		RootTasks[i]->UpdateListItem(OutTaskListItems[i]);
	}
}

void ATaskModule::SetRootTaskItem(int32 InIndex, URootTaskBase* InRootTask)
{
	if(RootTasks.IsValidIndex(InIndex))
	{
		RootTasks[InIndex] = InRootTask;
	}
}
#endif
