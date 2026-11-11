#include "Task/Base/TaskAssetReferenceTask.h"
#include "Task/TaskExecutionTypes.h"

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
	UTaskModule* Tasks = UTaskModule::GetPtr(GetWorld());
	if(!bRuntimeExpanded || !bAutoEnterReferencedRoots || !IsEntered() || !Tasks) return;
	for(UTaskBase* Task : SubTasks)
	{
		if(Task && Task->TaskState == ETaskState::None && Task->TaskEnterType != ETaskEnterType::None)
		{
			Tasks->EnterTask(Task, false);
		}
	}
}
