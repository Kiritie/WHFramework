// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Task/EventHandle_EndTask.h"

UEventHandle_EndTask::UEventHandle_EndTask()
{
	EventType = EEventType::Multicast;
}

void UEventHandle_EndTask::OnDespawn_Implementation(bool bRecovery)
{
}

void UEventHandle_EndTask::Fill_Implementation(const TArray<FParameter>& InParams)
{
}
