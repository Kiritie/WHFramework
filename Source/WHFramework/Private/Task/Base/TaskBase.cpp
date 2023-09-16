// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/Base/TaskBase.h"

#include "Camera/CameraModuleBPLibrary.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Task/EventHandle_CompleteTask.h"
#include "Event/Handle/Task/EventHandle_EnterTask.h"
#include "Event/Handle/Task/EventHandle_ExecuteTask.h"
#include "Event/Handle/Task/EventHandle_LeaveTask.h"
#include "Global/GlobalBPLibrary.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Task/TaskModule.h"
#include "Task/TaskModuleBPLibrary.h"

UTaskBase::UTaskBase()
{
	TaskGUID = FGuid::NewGuid().ToString();
	TaskDisplayName = FText::FromString(TEXT("Task Base"));
	TaskDescription = FText::GetEmpty();

	bFirstTask = false;
	TaskIndex = 0;
	TaskHierarchy = 0;
	TaskState = ETaskState::None;

	TaskEnterType = ETaskEnterType::Automatic;
	TaskExecuteResult = ETaskExecuteResult::None;

	TaskExecuteType = ETaskExecuteType::Automatic;
	AutoExecuteTaskTime = 0.f;

	TaskLeaveType = ETaskLeaveType::Automatic;
	AutoLeaveTaskTime = 0.f;

	TaskCompleteType = ETaskCompleteType::Procedure;
	AutoCompleteTaskTime = 0.f;
	TaskGuideType = ETaskGuideType::None;
	
	bTaskSkipAble = false;

	CurrentSubTaskIndex = -1;

	bMergeSubTask = false;

	SubTasks = TArray<UTaskBase*>();

	RootTask = nullptr;
	ParentTask = nullptr;

	TaskListItemStates = FTaskListItemStates();
}

#if WITH_EDITOR
void UTaskBase::OnGenerate()
{
	
}

void UTaskBase::OnUnGenerate()
{
	if(bFirstTask)
	{
		if(ATaskModule* TaskModule = ATaskModule::Get(true))
		{
			if(TaskModule->GetFirstTask() == this)
			{
				TaskModule->SetFirstTask(nullptr);
			}
		}
	}
}
#endif

void UTaskBase::OnStateChanged(ETaskState InTaskState)
{
	OnTaskStateChanged.Broadcast(InTaskState);
	K2_OnStateChanged(InTaskState);
}

void UTaskBase::OnInitialize()
{
	for (auto Iter : SubTasks)
	{
		if(Iter)
		{
			Iter->RootTask = IsRootTask() ? this : RootTask;
			Iter->ParentTask = this;
			Iter->OnInitialize();
		}
	}

	K2_OnInitialize();
}

void UTaskBase::OnRestore()
{
	TaskState = ETaskState::None;
	OnStateChanged(TaskState);

	CurrentSubTaskIndex = -1;

	K2_OnRestore();

	for(int32 i = SubTasks.Num() - 1; i >= 0; i--)
	{
		if(SubTasks[i])
		{
			SubTasks[i]->OnRestore();
		}
	}
}

void UTaskBase::OnEnter(UTaskBase* InLastTask)
{
	TaskState = ETaskState::Entered;
	OnStateChanged(TaskState);

	GetWorld()->GetTimerManager().ClearTimer(AutoExecuteTimerHandle);
	
	if(ParentTask)
	{
		ParentTask->CurrentSubTaskIndex = TaskIndex;
	}

	TaskExecuteResult = ETaskExecuteResult::None;

	WHDebug(FString::Printf(TEXT("进入任务: %s"), *TaskDisplayName.ToString()), EDebugMode::All, EDebugCategory::Task, EDebugVerbosity::Log, FColor::Cyan, 5.f);

	K2_OnEnter(InLastTask);

	switch(TaskGuideType)
	{
		case ETaskGuideType::TimerOnce:
		{
			GetWorld()->GetTimerManager().SetTimer(StartGuideTimerHandle, this, &UTaskBase::Guide, TaskGuideIntervalTime, false);
			break;
		}
		default: break;
	}

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_EnterTask::StaticClass(), EEventNetType::Single, this, {this});

	if(bMergeSubTask)
	{
		for (auto Iter : SubTasks)
		{
			if(Iter)
			{
				Iter->OnEnter(InLastTask);
			}
		}
	}

	if(TaskExecuteType == ETaskExecuteType::Automatic && TaskState != ETaskState::Executing)
	{
		if(AutoExecuteTaskTime > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(AutoExecuteTimerHandle, this, &UTaskBase::Execute, AutoExecuteTaskTime, false);
		}
		else
		{
			Execute();
		}
	}
}

void UTaskBase::OnRefresh()
{
	if(HasSubTask(false))
	{
		if(!IsAllSubCompleted())
		{
			const int32 Index = CurrentSubTaskIndex + 1;
			if(SubTasks.IsValidIndex(Index))
			{
				UTaskBase* SubTask = SubTasks[Index];
				if(SubTask)
				{
					if(SubTask->TaskEnterType == ETaskEnterType::Automatic)
					{
						SubTask->Enter();
					}
				}
			}
		}
		else
		{
			Complete(IsAllSubExecuteSucceed() ? ETaskExecuteResult::Succeed : ETaskExecuteResult::Failed);
		}
	}

	K2_OnRefresh();
}

void UTaskBase::OnGuide()
{
	switch(TaskGuideType)
	{
		case ETaskGuideType::TimerLoop:
		{
			GetWorld()->GetTimerManager().SetTimer(StartGuideTimerHandle, this, &UTaskBase::OnGuide, TaskGuideIntervalTime, false);
			break;
		}
		default: break;
	}

	K2_OnGuide();
}

void UTaskBase::OnExecute()
{
	TaskState = ETaskState::Executing;
	OnStateChanged(TaskState);

	GetWorld()->GetTimerManager().ClearTimer(AutoExecuteTimerHandle);

	K2_OnExecute();

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_ExecuteTask::StaticClass(), EEventNetType::Single, this, {this});

	if(TaskState != ETaskState::Completed)
	{
		switch(TaskCompleteType)
		{
			case ETaskCompleteType::Automatic:
			{
				if(!HasSubTask(false))
				{
					if(AutoCompleteTaskTime > 0.f)
					{
						GetWorld()->GetTimerManager().SetTimer(AutoCompleteTimerHandle, FTimerDelegate::CreateUObject(this, &UTaskBase::Complete, ETaskExecuteResult::Succeed), AutoCompleteTaskTime, false);
					}
					else
					{
						Complete();
					}
				}
				break;
			}
			case ETaskCompleteType::Skip:
			{
				Complete(ETaskExecuteResult::Skipped);
			}
			default: break;
		}
	}
}

void UTaskBase::OnComplete(ETaskExecuteResult InTaskExecuteResult)
{
	TaskState = ETaskState::Completed;
	OnStateChanged(TaskState);

	TaskExecuteResult = InTaskExecuteResult;

	GetWorld()->GetTimerManager().ClearTimer(AutoCompleteTimerHandle);

	GetWorld()->GetTimerManager().ClearTimer(StartGuideTimerHandle);
	
	K2_OnComplete(InTaskExecuteResult);

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_CompleteTask::StaticClass(), EEventNetType::Single, this, {this});

	if(TaskLeaveType == ETaskLeaveType::Automatic && TaskState != ETaskState::Leaved)
	{
		if(TaskExecuteResult != ETaskExecuteResult::Skipped && AutoLeaveTaskTime > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(AutoLeaveTimerHandle, this, &UTaskBase::Leave, AutoLeaveTaskTime, false);
		}
		else
		{
			Leave();
		}
	}
}

void UTaskBase::OnLeave()
{
	TaskState = ETaskState::Leaved;
	OnStateChanged(TaskState);

	GetWorld()->GetTimerManager().ClearTimer(AutoLeaveTimerHandle);
	
	WHDebug(FString::Printf(TEXT("%s任务: %s"), TaskExecuteResult != ETaskExecuteResult::Skipped ? TEXT("离开") : TEXT("跳过"), *TaskDisplayName.ToString()), EDebugMode::All, EDebugCategory::Task, EDebugVerbosity::Log, FColor::Orange, 5.f);

	K2_OnLeave();

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_LeaveTask::StaticClass(), EEventNetType::Single, this, {this});

	if(bMergeSubTask)
	{
		for(auto Iter : SubTasks)
		{
			if(Iter)
			{
				Iter->OnLeave();
			}
		}
	}
}

bool UTaskBase::IsEntered() const
{
	return TaskState == ETaskState::Entered || TaskState == ETaskState::Executing;
}

bool UTaskBase::IsCompleted(bool bCheckSubs) const
{
	return (TaskState == ETaskState::Completed || TaskState == ETaskState::Leaved) && (!bCheckSubs || IsAllSubCompleted());
}

bool UTaskBase::CheckTaskCondition_Implementation(FString& OutInfo) const
{
	return true;
}

bool UTaskBase::CheckTaskSkipAble_Implementation(FString& OutInfo) const
{
	return bTaskSkipAble;
}

float UTaskBase::CheckTaskProgress_Implementation(FString& OutInfo) const
{
	float Progress = 0.f;
	if(HasSubTask())
	{
		int32 Num = 0;
		for(auto Iter : SubTasks)
		{
			if(Iter->IsCompleted())
			{
				Num++;
			}
		}
		Progress = (float)Num / SubTasks.Num();
		OutInfo = FString::Printf(TEXT("%d/%d"), Num, SubTasks.Num());
	}
	return Progress;
}

bool UTaskBase::IsParentOf(UTaskBase* InTask) const
{
	if(InTask && InTask->ParentTask)
	{
		if(InTask->ParentTask == this) return true;
		return InTask->ParentTask->IsParentOf(InTask);
	}
	return false;
}

void UTaskBase::Restore()
{
	UTaskModuleBPLibrary::RestoreTask(this);
}

void UTaskBase::Enter()
{
	UTaskModuleBPLibrary::EnterTask(this);
}

void UTaskBase::Refresh()
{
	UTaskModuleBPLibrary::RefreshTask(this);
}

void UTaskBase::Guide()
{
	UTaskModuleBPLibrary::GuideTask(this);
}

void UTaskBase::Execute()
{
	UTaskModuleBPLibrary::ExecuteTask(this);
}

void UTaskBase::Complete(ETaskExecuteResult InTaskExecuteResult)
{
	UTaskModuleBPLibrary::CompleteTask(this, InTaskExecuteResult);
}

void UTaskBase::Leave()
{
	UTaskModuleBPLibrary::LeaveTask(this);
}

void UTaskBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.ArIsSaveGame)
	{
		if(Ar.IsLoading())
		{
			Ar << TaskState;
			Ar << TaskExecuteResult;
			Ar << CurrentSubTaskIndex;
		}
		else if(Ar.IsSaving())
		{
			Ar << TaskState;
			Ar << TaskExecuteResult;
			Ar << CurrentSubTaskIndex;
		}
	}
}

void UTaskBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	switch(TaskState)
	{
		case ETaskState::Entered:
		{
			OnEnter(nullptr);
			break;
		}
		case ETaskState::Executing:
		{
			OnExecute();
			break;
		}
		case ETaskState::Completed:
		{
			OnComplete(TaskExecuteResult);
			break;
		}
		case ETaskState::Leaved:
		{
			OnLeave();
			break;
		}
		default: break;
	}

}

FSaveData* UTaskBase::ToData(bool bRefresh)
{
	static FSaveData SaveData;
	SaveData = FSaveData();

	return &SaveData;
}

bool UTaskBase::HasSubTask(bool bIgnoreMerge) const
{
	return SubTasks.Num() > 0 && (bIgnoreMerge || !bMergeSubTask);
}

UTaskBase* UTaskBase::GetCurrentSubTask() const
{
	if(SubTasks.IsValidIndex(CurrentSubTaskIndex))
	{
		return SubTasks[CurrentSubTaskIndex];
	}
	return nullptr;
}

bool UTaskBase::IsSubOf(UTaskBase* InTask) const
{
	if(InTask && ParentTask)
	{
		if(InTask == ParentTask) return true;
		return ParentTask->IsSubOf(InTask);
	}
	return false;
}

bool UTaskBase::IsAllSubCompleted() const
{
	for (auto Iter : SubTasks)
	{
		if(Iter && !Iter->IsCompleted())
		{
			return false;
		}
	}
	return true;
}

bool UTaskBase::IsAllSubExecuteSucceed() const
{
	for (auto Iter : SubTasks)
	{
		if(Iter && Iter->TaskExecuteResult == ETaskExecuteResult::Failed)
		{
			return false;
		}
	}
	return true;
}

void UTaskBase::GenerateListItem(TSharedPtr<FTaskListItem> OutTaskListItem)
{
	OutTaskListItem->Task = this;
	for (int32 i = 0; i < SubTasks.Num(); i++)
	{
		if(SubTasks[i])
		{
			auto Item = MakeShared<FTaskListItem>();
			Item->ParentListItem = OutTaskListItem;
			OutTaskListItem->SubListItems.Add(Item);
			SubTasks[i]->GenerateListItem(Item);
		}
	}
}

void UTaskBase::UpdateListItem(TSharedPtr<FTaskListItem> OutTaskListItem)
{
	OutTaskListItem->Task = this;
	for (int32 i = 0; i < SubTasks.Num(); i++)
	{
		if(SubTasks[i])
		{
			SubTasks[i]->TaskIndex = i;
			SubTasks[i]->TaskHierarchy = TaskHierarchy + 1;
			SubTasks[i]->RootTask = IsRootTask() ? this : RootTask;
			SubTasks[i]->ParentTask = this;
			SubTasks[i]->UpdateListItem(OutTaskListItem->SubListItems[i]);
		}
	}
}
#if WITH_EDITOR
bool UTaskBase::CanEditChange(const FProperty* InProperty) const
{
	if(InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, TaskExecuteType) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, TaskCompleteType) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, TaskLeaveType) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, TaskGuideType))
		{
			return !HasSubTask(false);
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, AutoExecuteTaskTime))
		{
			return TaskExecuteType == ETaskExecuteType::Automatic;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, AutoCompleteTaskTime))
		{
			return TaskCompleteType == ETaskCompleteType::Automatic;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, AutoLeaveTaskTime))
		{
			return TaskLeaveType == ETaskLeaveType::Automatic;
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, TaskGuideIntervalTime))
		{
			return !HasSubTask(false) && (TaskGuideType == ETaskGuideType::TimerOnce || TaskGuideType == ETaskGuideType::TimerLoop);
		}
	}

	return Super::CanEditChange(InProperty);
}

void UTaskBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* Property = PropertyChangedEvent.MemberProperty;

	if(Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		auto PropertyName = Property->GetFName();

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, SubTasks) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, bMergeSubTask))
		{
			if(HasSubTask(false))
			{
				TaskExecuteType = ETaskExecuteType::Automatic;
				TaskCompleteType = ETaskCompleteType::Procedure;
				AutoCompleteTaskTime = 0.f;
				TaskLeaveType = ETaskLeaveType::Automatic;
				TaskGuideType = ETaskGuideType::None;				
				TaskGuideIntervalTime = 0.f;
			}
		}

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTaskBase, bFirstTask))
		{
			if(ATaskModule* TaskModule = ATaskModule::Get(true))
			{
				if(bFirstTask)
				{
					if(TaskModule->GetFirstTask())
					{
						TaskModule->GetFirstTask()->bFirstTask = false;
					}
					TaskModule->SetFirstTask(this);
				}
				else if(TaskModule->GetFirstTask() == this)
				{
					TaskModule->SetFirstTask(nullptr);
				}
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
