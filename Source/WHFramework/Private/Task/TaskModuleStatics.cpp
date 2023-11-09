// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/TaskModuleStatics.h"

#include "Task/TaskModule.h"
#include "Task/Base/TaskBase.h"

UTaskBase* UTaskModuleStatics::GetCurrentTask()
{
	if(UTaskModule* TaskModule = UTaskModule::Get())
	{
		return TaskModule->GetCurrentTask();
	}
	return nullptr;
}

UTaskBase* UTaskModuleStatics::GetCurrentRootTask()
{
	if(UTaskModule* TaskModule = UTaskModule::Get())
	{
		return TaskModule->GetCurrentRootTask();
	}
	return nullptr;
}

UTaskBase* UTaskModuleStatics::GetTaskByGUID(const FString& InTaskGUID)
{
	if(UTaskModule* TaskModule = UTaskModule::Get())
	{
		return TaskModule->GetTaskByGUID(InTaskGUID);
	}
	return nullptr;
}

void UTaskModuleStatics::RestoreTask(UTaskBase* InTask)
{
	if(UTaskModule* TaskModule = UTaskModule::Get())
	{
		TaskModule->RestoreTask(InTask);
	}
}

void UTaskModuleStatics::EnterTask(UTaskBase* InTask)
{
	if(UTaskModule* TaskModule = UTaskModule::Get())
	{
		TaskModule->EnterTask(InTask);
	}
}

void UTaskModuleStatics::RefreshTask(UTaskBase* InTask)
{
	if(UTaskModule* TaskModule = UTaskModule::Get())
	{
		TaskModule->RefreshTask(InTask);
	}
}

void UTaskModuleStatics::GuideTask(UTaskBase* InTask)
{
	if(UTaskModule* TaskModule = UTaskModule::Get())
	{
		TaskModule->GuideTask(InTask);
	}
}

void UTaskModuleStatics::ExecuteTask(UTaskBase* InTask)
{
	if(UTaskModule* TaskModule = UTaskModule::Get())
	{
		TaskModule->ExecuteTask(InTask);
	}
}

void UTaskModuleStatics::CompleteTask(UTaskBase* InTask, ETaskExecuteResult InTaskExecuteResult)
{
	if(UTaskModule* TaskModule = UTaskModule::Get())
	{
		TaskModule->CompleteTask(InTask, InTaskExecuteResult);
	}
}

void UTaskModuleStatics::LeaveTask(UTaskBase* InTask)
{
	if(UTaskModule* TaskModule = UTaskModule::Get())
	{
		TaskModule->LeaveTask(InTask);
	}
}
