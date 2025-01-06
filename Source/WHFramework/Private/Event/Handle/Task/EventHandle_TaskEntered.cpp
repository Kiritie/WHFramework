// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Task/EventHandle_TaskEntered.h"

UEventHandle_TaskEntered::UEventHandle_TaskEntered()
{
	Task = nullptr;
}

void UEventHandle_TaskEntered::OnDespawn_Implementation(bool bRecovery)
{
	Task = nullptr;
}

void UEventHandle_TaskEntered::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Task = InParams[0];
	}
}

TArray<FParameter> UEventHandle_TaskEntered::Pack_Implementation()
{
	return { Task };
}
