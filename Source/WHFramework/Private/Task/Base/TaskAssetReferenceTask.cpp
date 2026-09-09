#include "Task/Base/TaskAssetReferenceTask.h"

#include "Task/TaskModule.h"

UTaskAssetReferenceTask::UTaskAssetReferenceTask()
{
	TaskDisplayName = NSLOCTEXT("Task", "TaskAssetReference", "Task Asset");
	TaskEnterType = ETaskEnterType::Automatic;
	TaskExecuteType = ETaskExecuteType::Automatic;
	TaskCompleteType = ETaskCompleteType::Procedure;
	TaskLeaveType = ETaskLeaveType::Automatic;
}

void UTaskAssetReferenceTask::OnEnter()
{
	Super::OnEnter();
	EnterReferencedRoots();
}

void UTaskAssetReferenceTask::OnRefresh()
{
	EnterReferencedRoots();
	Super::OnRefresh();
}

bool UTaskAssetReferenceTask::AreSubTasksReadyToComplete() const
{
	if(SubTasks.IsEmpty()) return false;
	for(const UTaskBase* Task : SubTasks)
	{
		if(!Task || Task->TaskState != ETaskState::Leaved) return false;
	}
	return true;
}

void UTaskAssetReferenceTask::EnterReferencedRoots()
{
	if(!bRuntimeExpanded || !bAutoEnterReferencedRoots || !IsEntered() || !UTaskModule::IsValid()) return;
	for(UTaskBase* Task : SubTasks)
	{
		if(Task && Task->TaskState == ETaskState::None && Task->TaskEnterType != ETaskEnterType::None)
		{
			UTaskModule::Get().EnterTask(Task, false);
		}
	}
}
