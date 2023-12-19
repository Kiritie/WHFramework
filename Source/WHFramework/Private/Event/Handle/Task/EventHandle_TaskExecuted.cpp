// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Task/EventHandle_TaskExecuted.h"

#include "Task/Base/TaskBase.h"

UEventHandle_TaskExecuted::UEventHandle_TaskExecuted()
{

}

void UEventHandle_TaskExecuted::OnDespawn_Implementation(bool bRecovery)
{
	Task = nullptr;
}

void UEventHandle_TaskExecuted::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Task = InParams[0].GetObjectValue<UTaskBase>();
	}
}
