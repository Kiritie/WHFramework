// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Task/EventHandle_StartTask.h"

UEventHandle_StartTask::UEventHandle_StartTask()
{
	EventType = EEventType::Multicast;
	RootTaskIndex = -1;
}

void UEventHandle_StartTask::OnDespawn_Implementation(bool bRecovery)
{
	RootTaskIndex = -1;
}

void UEventHandle_StartTask::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		RootTaskIndex = InParams[0].GetIntegerValue();
	}
}
