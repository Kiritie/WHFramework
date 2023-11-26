// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Task/EventHandle_CompleteTask.h"

UEventHandle_CompleteTask::UEventHandle_CompleteTask()
{
	Task = nullptr;
}

void UEventHandle_CompleteTask::OnDespawn_Implementation(bool bRecovery)
{
	Task = nullptr;
}

void UEventHandle_CompleteTask::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Task = InParams[0].GetObjectValue<UTaskBase>();
	}
}
