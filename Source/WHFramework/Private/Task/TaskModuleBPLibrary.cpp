// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/TaskModuleBPLibrary.h"

#include "Task/TaskModule.h"
#include "Task/Base/TaskBase.h"

UTaskBase* UTaskModuleBPLibrary::GetCurrentTask()
{
	if(ATaskModule* TaskModule = ATaskModule::Get())
	{
		return TaskModule->GetCurrentTask();
	}
	return nullptr;
}

UTaskBase* UTaskModuleBPLibrary::GetCurrentRootTask()
{
	if(ATaskModule* TaskModule = ATaskModule::Get())
	{
		return TaskModule->GetCurrentRootTask();
	}
	return nullptr;
}

UTaskBase* UTaskModuleBPLibrary::GetTaskByGUID(const FString& InTaskGUID)
{
	if(ATaskModule* TaskModule = ATaskModule::Get())
	{
		return TaskModule->GetTaskByGUID(InTaskGUID);
	}
	return nullptr;
}

void UTaskModuleBPLibrary::RestoreTask(UTaskBase* InTask)
{
	if(ATaskModule* TaskModule = ATaskModule::Get())
	{
		TaskModule->RestoreTask(InTask);
	}
}

void UTaskModuleBPLibrary::EnterTask(UTaskBase* InTask)
{
	if(ATaskModule* TaskModule = ATaskModule::Get())
	{
		TaskModule->EnterTask(InTask);
	}
}

void UTaskModuleBPLibrary::RefreshTask(UTaskBase* InTask)
{
	if(ATaskModule* TaskModule = ATaskModule::Get())
	{
		TaskModule->RefreshTask(InTask);
	}
}

void UTaskModuleBPLibrary::GuideTask(UTaskBase* InTask)
{
	if(ATaskModule* TaskModule = ATaskModule::Get())
	{
		TaskModule->GuideTask(InTask);
	}
}

void UTaskModuleBPLibrary::ExecuteTask(UTaskBase* InTask)
{
	if(ATaskModule* TaskModule = ATaskModule::Get())
	{
		TaskModule->ExecuteTask(InTask);
	}
}

void UTaskModuleBPLibrary::CompleteTask(UTaskBase* InTask, ETaskExecuteResult InTaskExecuteResult)
{
	if(ATaskModule* TaskModule = ATaskModule::Get())
	{
		TaskModule->CompleteTask(InTask, InTaskExecuteResult);
	}
}

void UTaskModuleBPLibrary::LeaveTask(UTaskBase* InTask)
{
	if(ATaskModule* TaskModule = ATaskModule::Get())
	{
		TaskModule->LeaveTask(InTask);
	}
}
