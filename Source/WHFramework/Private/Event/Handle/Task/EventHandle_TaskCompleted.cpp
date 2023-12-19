// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Task/EventHandle_TaskCompleted.h"

UEventHandle_TaskCompleted::UEventHandle_TaskCompleted()
{
	Task = nullptr;
}

void UEventHandle_TaskCompleted::OnDespawn_Implementation(bool bRecovery)
{
	Task = nullptr;
}

void UEventHandle_TaskCompleted::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Task = InParams[0].GetObjectValue<UTaskBase>();
	}
}
