// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/TaskModuleStatics.h"

#include "Task/TaskModule.h"
#include "Task/Base/TaskBase.h"

UTaskAsset* UTaskModuleStatics::GetCurrentTaskAsset()
{
	return UTaskModule::Get().GetCurrentAsset();
}

void UTaskModuleStatics::SetCurrentTaskAsset(UTaskAsset* InTaskAsset, bool bAutoEnterFirst)
{
	return UTaskModule::Get().SetCurrentAsset(InTaskAsset, bAutoEnterFirst);
}

UTaskBase* UTaskModuleStatics::GetCurrentTask()
{
	return UTaskModule::Get().GetCurrentTask();
}

UTaskBase* UTaskModuleStatics::GetCurrentRootTask()
{
	return UTaskModule::Get().GetCurrentRootTask();
}

UTaskBase* UTaskModuleStatics::GetTaskByGUID(const FString& InTaskGUID)
{
	return UTaskModule::Get().GetTaskByGUID(InTaskGUID);
}

void UTaskModuleStatics::RestoreTask(UTaskBase* InTask)
{
	UTaskModule::Get().RestoreTask(InTask);
}

void UTaskModuleStatics::EnterTask(UTaskBase* InTask)
{
	UTaskModule::Get().EnterTask(InTask);
}

void UTaskModuleStatics::RefreshTask(UTaskBase* InTask)
{
	UTaskModule::Get().RefreshTask(InTask);
}

void UTaskModuleStatics::GuideTask(UTaskBase* InTask)
{
	UTaskModule::Get().GuideTask(InTask);
}

void UTaskModuleStatics::ExecuteTask(UTaskBase* InTask)
{
	UTaskModule::Get().ExecuteTask(InTask);
}

void UTaskModuleStatics::CompleteTask(UTaskBase* InTask, ETaskExecuteResult InTaskExecuteResult)
{
	UTaskModule::Get().CompleteTask(InTask, InTaskExecuteResult);
}

void UTaskModuleStatics::LeaveTask(UTaskBase* InTask)
{
	UTaskModule::Get().LeaveTask(InTask);
}
