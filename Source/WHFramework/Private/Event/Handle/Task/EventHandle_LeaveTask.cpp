// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Task/EventHandle_LeaveTask.h"

UEventHandle_LeaveTask::UEventHandle_LeaveTask()
{
	EventType = EEventType::Multicast;
}

void UEventHandle_LeaveTask::OnDespawn_Implementation(bool bRecovery)
{
	Task = nullptr;
}

void UEventHandle_LeaveTask::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Task = InParams[0].GetObjectValue<UTaskBase>();
	}
}
