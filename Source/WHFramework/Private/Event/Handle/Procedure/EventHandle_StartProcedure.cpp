// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Procedure/EventHandle_StartProcedure.h"

UEventHandle_StartProcedure::UEventHandle_StartProcedure()
{
	EventType = EEventType::Multicast;
	ProcedureIndex = -1;
}

void UEventHandle_StartProcedure::OnDespawn_Implementation()
{
	ProcedureIndex = -1;
}

void UEventHandle_StartProcedure::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		ProcedureIndex = InParams[0].GetIntegerValue();
	}
}
