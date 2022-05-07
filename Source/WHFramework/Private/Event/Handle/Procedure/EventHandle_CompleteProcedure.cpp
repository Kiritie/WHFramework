// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Procedure/EventHandle_CompleteProcedure.h"

UEventHandle_CompleteProcedure::UEventHandle_CompleteProcedure()
{
	EventType = EEventType::Multicast;
	Procedure = nullptr;
}

void UEventHandle_CompleteProcedure::OnDespawn_Implementation()
{
	Procedure = nullptr;
}

void UEventHandle_CompleteProcedure::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Procedure = InParams[0].GetObjectValue<UProcedureBase>();
	}
}
