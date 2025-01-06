// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Task/EventHandle_TaskStateChanged.h"

UEventHandle_TaskStateChanged::UEventHandle_TaskStateChanged()
{
	Task = nullptr;
}

void UEventHandle_TaskStateChanged::OnDespawn_Implementation(bool bRecovery)
{
	Task = nullptr;
}

void UEventHandle_TaskStateChanged::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Task = InParams[0];
	}
}

TArray<FParameter> UEventHandle_TaskStateChanged::Pack_Implementation()
{
	return { Task };
}
