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
#include "Event/Handle/Task/EventHandle_EndTask.h"
#include "Event/Handle/Task/EventHandle_StartTask.h"
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

	bAutoStartFirst = false;

	CurrentRootTaskIndex = -1;
	RootTasks = TArray<URootTaskBase*>();

	FirstTask = nullptr;
	CurrentTask = nullptr;
	TaskModuleState = ETaskModuleState::None;

	GlobalTaskExecuteType = ETaskExecuteType::None;
	GlobalTaskLeaveType = ETaskLeaveType::None;
	GlobalTaskCompleteType = ETaskCompleteType::None;
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

	if(InPhase == EPhase::Final && bAutoStartFirst)
	{
		StartTask(-1, true);
	}
}

void ATaskModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	if(TaskModuleState == ETaskModuleState::Running)
	{
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
					else if(CurrentTask->ParentTask->IsA<URootTaskBase>())
					{
						StartTask(CurrentTask->ParentTask->TaskIndex + 1, false);
					}
				}
			}
			else
			{
				if(CurrentTask->GetTaskState() != ETaskState::Leaved)
				{
					CurrentTask->Refresh();
				}
				else
				{
					StartTask(CurrentTask->TaskIndex + 1, false);
				}
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

	EndTask();
}

void ATaskModule::StartTask(int32 InRootTaskIndex, bool bSkipTasks)
{
	InRootTaskIndex = InRootTaskIndex != -1 ? InRootTaskIndex : FirstTask && FirstTask->IsA<URootTaskBase>() ? FirstTask->TaskIndex : -1;
	if(InRootTaskIndex != -1)
	{
		if(RootTasks.IsValidIndex(InRootTaskIndex))
		{
			if(TaskModuleState != ETaskModuleState::Running)
			{
				TaskModuleState = ETaskModuleState::Running;
				UEventModuleBPLibrary::BroadcastEvent(UEventHandle_StartTask::StaticClass(), EEventNetType::Single, this, {FParameter::MakeInteger(InRootTaskIndex)});
			}

            for(int32 i = CurrentRootTaskIndex; i <= InRootTaskIndex; i++)
            {
            	if(RootTasks.IsValidIndex(i) && RootTasks[i])
            	{
            		if(i == InRootTaskIndex)
            		{
            			RootTasks[i]->Enter();
            		}
            		else if(bSkipTasks)
            		{
            			RootTasks[i]->Complete(ETaskExecuteResult::Skipped);
            		}
            	}
            }
            CurrentRootTaskIndex = InRootTaskIndex;
		}
	}
	else if(FirstTask)
	{
		InRootTaskIndex = FirstTask->RootTask->TaskIndex;
		
		if(RootTasks.IsValidIndex(InRootTaskIndex))
		{
			if(TaskModuleState != ETaskModuleState::Running)
			{
				TaskModuleState = ETaskModuleState::Running;
				UEventModuleBPLibrary::BroadcastEvent(UEventHandle_StartTask::StaticClass(), EEventNetType::Single, this, {FParameter::MakeInteger(InRootTaskIndex)});
			}
		
			if(bSkipTasks)
			{
				for(int32 i = CurrentRootTaskIndex; i <= InRootTaskIndex; i++)
				{
					if(RootTasks.IsValidIndex(i) && RootTasks[i])
					{
						if(i == InRootTaskIndex)
						{
							RootTasks[i]->Enter();
							RootTasks[i]->Execute();
							for(int32 j = 0; j < FirstTask->TaskIndex; j++)
							{
								FirstTask->ParentTask->SubTasks[j]->Complete(ETaskExecuteResult::Skipped);
							}
						}
						else
						{
							RootTasks[i]->Complete(ETaskExecuteResult::Skipped);
						}
					}
				}
			}
			FirstTask->Enter();
		
			CurrentRootTaskIndex = InRootTaskIndex;
		}
	}
}

void ATaskModule::EndTask(bool bRestoreTasks)
{
	if(TaskModuleState == ETaskModuleState::Running)
	{
		TaskModuleState = ETaskModuleState::Ended;
		UEventModuleBPLibrary::BroadcastEvent<UEventHandle_EndTask>(EEventNetType::Single, this);
	}

	for(int32 i = CurrentRootTaskIndex; i >= 0; i--)
	{
		if(RootTasks.IsValidIndex(i) && RootTasks[i])
		{
			if(i == CurrentRootTaskIndex)
			{
				RootTasks[i]->Complete(ETaskExecuteResult::Skipped);
			}
			if(bRestoreTasks)
			{
				RootTasks[i]->Restore();
			}
		}
	}
	if(bRestoreTasks)
	{
		CurrentRootTaskIndex = -1;
		CurrentTask = nullptr;
	}
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

		if(InTask->IsA<URootTaskBase>())
		{
			CurrentRootTaskIndex = InTask->TaskIndex;
		}

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
		InTask->OnComplete(InTaskExecuteResult);
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
		if(InTask->IsA<URootTaskBase>())
		{
			if(InTask->TaskIndex == RootTasks.Num() - 1)
			{
				EndTask();
			}
		}
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

void ATaskModule::SetGlobalTaskExecuteType(ETaskExecuteType InGlobalTaskExecuteType)
{
	if(GlobalTaskExecuteType != InGlobalTaskExecuteType)
	{
		GlobalTaskExecuteType = InGlobalTaskExecuteType;
		if(CurrentTask && CurrentTask->GetTaskExecuteType() == ETaskExecuteType::Automatic && CurrentTask->GetTaskState() == ETaskState::Entered)
		{
			CurrentTask->Execute();
		}
	}
}

void ATaskModule::SetGlobalTaskCompleteType(ETaskCompleteType InGlobalTaskCompleteType)
{
	if(GlobalTaskCompleteType != InGlobalTaskCompleteType)
	{
		GlobalTaskCompleteType = InGlobalTaskCompleteType;
		if(CurrentTask && CurrentTask->GetTaskCompleteType() == ETaskCompleteType::Automatic && CurrentTask->GetTaskState() == ETaskState::Executing)
		{
			CurrentTask->Complete();
		}
	}
}

void ATaskModule::SetGlobalTaskLeaveType(ETaskLeaveType InGlobalTaskLeaveType)
{
	if(GlobalTaskLeaveType != InGlobalTaskLeaveType)
	{
		GlobalTaskLeaveType = InGlobalTaskLeaveType;
		if(CurrentTask && CurrentTask->GetTaskLeaveType() == ETaskLeaveType::Automatic && CurrentTask->GetTaskState() == ETaskState::Completed)
		{
			CurrentTask->Leave();
		}
	}
}
