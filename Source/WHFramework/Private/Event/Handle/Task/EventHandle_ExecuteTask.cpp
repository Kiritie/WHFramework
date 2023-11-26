// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Task/EventHandle_ExecuteTask.h"

#include "Task/Base/TaskBase.h"

UEventHandle_ExecuteTask::UEventHandle_ExecuteTask()
{

}

void UEventHandle_ExecuteTask::OnDespawn_Implementation(bool bRecovery)
{
	Task = nullptr;
}

void UEventHandle_ExecuteTask::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Task = InParams[0].GetObjectValue<UTaskBase>();
	}
}
