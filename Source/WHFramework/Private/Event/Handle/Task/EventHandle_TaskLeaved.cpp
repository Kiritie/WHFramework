// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Task/EventHandle_TaskLeaved.h"

UEventHandle_TaskLeaved::UEventHandle_TaskLeaved()
{

}

void UEventHandle_TaskLeaved::OnDespawn_Implementation(bool bRecovery)
{
	Task = nullptr;
}

void UEventHandle_TaskLeaved::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Task = InParams[0].GetObjectValue<UTaskBase>();
	}
}
