// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Task/EventHandle_EnterTask.h"

UEventHandle_EnterTask::UEventHandle_EnterTask()
{
	EventType = EEventType::Multicast;
	Task = nullptr;
}

void UEventHandle_EnterTask::OnDespawn_Implementation(bool bRecovery)
{
	Task = nullptr;
}

void UEventHandle_EnterTask::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Task = InParams[0].GetObjectValue<UTaskBase>();
	}
}
