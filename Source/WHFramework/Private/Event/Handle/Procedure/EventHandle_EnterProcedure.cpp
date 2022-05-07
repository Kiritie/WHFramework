// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Procedure/EventHandle_EnterProcedure.h"

UEventHandle_EnterProcedure::UEventHandle_EnterProcedure()
{
	EventType = EEventType::Multicast;
	Procedure = nullptr;
}

void UEventHandle_EnterProcedure::OnDespawn_Implementation()
{
	Procedure = nullptr;
}

void UEventHandle_EnterProcedure::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Procedure = InParams[0].GetObjectValue<UProcedureBase>();
	}
}
