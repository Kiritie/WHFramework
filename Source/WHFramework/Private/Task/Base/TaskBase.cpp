// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/Base/TaskBase.h"

#include "Event/EventModuleStatics.h"
#include "Event/Handle/Task/EventHandle_TaskCompleted.h"
#include "Event/Handle/Task/EventHandle_TaskEntered.h"
#include "Event/Handle/Task/EventHandle_TaskExecuted.h"
#include "Event/Handle/Task/EventHandle_TaskLeaved.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/Handle/Task/EventHandle_TaskStateChanged.h"
#include "Task/TaskModule.h"
#include "Task/TaskModuleStatics.h"

UTaskBase::UTaskBase()
{
	TaskGUID = FGuid::NewGuid().ToString();
	TaskDisplayName = FText::FromString(TEXT("Task Base"));
	TaskDescription = FText::GetEmpty();

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
	TaskGuideIntervalTime = 0.f;
	
	bTaskSkipAble = false;

	bMergeSubTask = false;

	SubTasks = TArray<UTaskBase*>();

	RootTask = nullptr;
	ParentTask = nullptr;
}

#if WITH_EDITOR
void UTaskBase::OnGenerate()
{
	
}

void UTaskBase::OnUnGenerate()
{
	
}
#endif

void UTaskBase::OnStateChanged(ETaskState InTaskState)
{
	OnTaskStateChanged.Broadcast(InTaskState);
	K2_OnStateChanged(InTaskState);

	UEventModuleStatics::BroadcastEvent(UEventHandle_TaskStateChanged::StaticClass(), this, {this});
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

	K2_OnRestore();

	for(auto Iter : SubTasks)
	{
		if(Iter)
		{
			Iter->OnRestore();
		}
	}
}

void UTaskBase::OnEnter()
{
	TaskState = ETaskState::Entered;
	OnStateChanged(TaskState);

	GetWorld()->GetTimerManager().ClearTimer(AutoExecuteTimerHandle);

	TaskExecuteResult = ETaskExecuteResult::None;

	WHDebug(FString::Printf(TEXT("进入任务: %s"), *TaskDisplayName.ToString()), EDM_All, EDC_Task, EDV_Log, FColor::Cyan, 5.f);

	K2_OnEnter();

	switch(TaskGuideType)
	{
		case ETaskGuideType::TimerOnce:
		{
			GetWorld()->GetTimerManager().SetTimer(StartGuideTimerHandle, this, &UTaskBase::Guide, TaskGuideIntervalTime, false);
			break;
		}
		default: break;
	}

	UEventModuleStatics::BroadcastEvent(UEventHandle_TaskEntered::StaticClass(), this, {this});

	if(bMergeSubTask)
	{
		for (auto Iter : SubTasks)
		{
			if(Iter)
			{
				Iter->OnEnter();
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
	K2_OnRefresh();

	for (auto Iter : SubTasks)
	{
		if(Iter)
		{
			Iter->Refresh();
		}
	}
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

	UEventModuleStatics::BroadcastEvent(UEventHandle_TaskExecuted::StaticClass(), this, {this});

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
	for(auto Iter : SubTasks)
	{
		if(Iter)
		{
			Iter->Complete(InTaskExecuteResult);
		}
	}

	TaskState = ETaskState::Completed;
	OnStateChanged(TaskState);

	TaskExecuteResult = InTaskExecuteResult;

	GetWorld()->GetTimerManager().ClearTimer(AutoCompleteTimerHandle);

	GetWorld()->GetTimerManager().ClearTimer(StartGuideTimerHandle);
	
	K2_OnComplete(InTaskExecuteResult);

	UEventModuleStatics::BroadcastEvent(UEventHandle_TaskCompleted::StaticClass(), this, {this});

	if(InTaskExecuteResult != ETaskExecuteResult::Skipped)
	{
		WHDebug(FString::Printf(TEXT("任务%s: %s"), TaskExecuteResult != ETaskExecuteResult::Skipped ? TEXT("完成") : TEXT("失败"), *TaskDisplayName.ToString()), EDM_All, EDC_Task, EDV_Log, FColor::Green, 5.f);
	}

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
	for(auto Iter : SubTasks)
	{
		if(Iter)
		{
			Iter->Leave();
		}
	}

	TaskState = ETaskState::Leaved;
	OnStateChanged(TaskState);

	GetWorld()->GetTimerManager().ClearTimer(AutoLeaveTimerHandle);
	
	WHDebug(FString::Printf(TEXT("%s任务: %s"), TaskExecuteResult != ETaskExecuteResult::Skipped ? TEXT("离开") : TEXT("跳过"), *TaskDisplayName.ToString()), EDM_All, EDC_Task, EDV_Log, FColor::Orange, 5.f);

	K2_OnLeave();

	UEventModuleStatics::BroadcastEvent(UEventHandle_TaskLeaved::StaticClass(), this, {this});
}

UTaskAsset* UTaskBase::GetTaskAsset() const
{
	return Cast<UTaskAsset>(GetOuter());
}

bool UTaskBase::IsCurrent() const
{
	return UTaskModuleStatics::GetCurrentTask() == this;
}

bool UTaskBase::IsEntered() const
{
	return TaskState == ETaskState::Entered || TaskState == ETaskState::Executing;
}

bool UTaskBase::IsExecuting() const
{
	return TaskState == ETaskState::Executing;
}

bool UTaskBase::IsCompleted(bool bCheckSubs) const
{
	return (TaskState == ETaskState::Completed || TaskState == ETaskState::Leaved) && (!bCheckSubs || IsAllSubCompleted());
}

bool UTaskBase::IsLeaved() const
{
	return TaskState == ETaskState::None || TaskState == ETaskState::Leaved;
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
		return IsParentOf(InTask->ParentTask);
	}
	return false;
}

void UTaskBase::Restore()
{
	UTaskModuleStatics::RestoreTask(this);
}

void UTaskBase::Enter(bool bSetAsCurrent)
{
	UTaskModuleStatics::EnterTask(this, bSetAsCurrent);
}

void UTaskBase::Refresh()
{
	UTaskModuleStatics::RefreshTask(this);
}

void UTaskBase::RefreshState()
{
	OnStateChanged(TaskState);
}

void UTaskBase::Guide()
{
	UTaskModuleStatics::GuideTask(this);
}

void UTaskBase::Execute()
{
	UTaskModuleStatics::ExecuteTask(this);
}

void UTaskBase::Complete(ETaskExecuteResult InTaskExecuteResult)
{
	UTaskModuleStatics::CompleteTask(this, InTaskExecuteResult);
}

void UTaskBase::Leave()
{
	UTaskModuleStatics::LeaveTask(this);
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
		}
		else if(Ar.IsSaving())
		{
			Ar << TaskState;
			Ar << TaskExecuteResult;
		}
	}
}

void UTaskBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	switch(TaskState)
	{
		case ETaskState::Entered:
		{
			OnEnter();
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

FSaveData* UTaskBase::ToData()
{
	static FSaveData SaveData;
	SaveData = FSaveData();

	return &SaveData;
}

bool UTaskBase::HasSubTask(bool bIgnoreMerge) const
{
	return SubTasks.Num() > 0 && (bIgnoreMerge || !bMergeSubTask);
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

#if WITH_EDITOR
bool UTaskBase::GenerateListItem(TSharedPtr<FTaskListItem> OutTaskListItem, const FString& InFilterText)
{
	OutTaskListItem->Task = this;
	for (int32 i = 0; i < SubTasks.Num(); i++)
	{
		if(SubTasks[i])
		{
			auto Item = MakeShared<FTaskListItem>();
			Item->ParentListItem = OutTaskListItem;
			if(SubTasks[i]->GenerateListItem(Item, InFilterText))
			{
				OutTaskListItem->SubListItems.Add(Item);
			}
		}
	}
	if(!InFilterText.IsEmpty())
	{
		OutTaskListItem->GetStates().bExpanded = true;
		return TaskDisplayName.ToString().Contains(InFilterText);
	}
	return true;
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

bool UTaskBase::CanEditChange(const FProperty* InProperty) const
{
	if(InProperty)
	{
		const FString PropertyName = InProperty->GetName();

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
	const FProperty* Property = PropertyChangedEvent.MemberProperty;

	if(Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		const FName PropertyName = Property->GetFName();

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
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
