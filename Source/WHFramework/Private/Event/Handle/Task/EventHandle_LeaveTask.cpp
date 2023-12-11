// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Task/EventHandle_LeaveTask.h"

UEventHandle_LeaveTask::UEventHandle_LeaveTask()
{

}

void UEventHandle_LeaveTask::OnDespawn_Implementation(bool bRecovery)
{
	Task = nullptr;
}

void UEventHandle_LeaveTask::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Task = InParams[0].GetObjectValue<UTaskBase>();
	}
}
