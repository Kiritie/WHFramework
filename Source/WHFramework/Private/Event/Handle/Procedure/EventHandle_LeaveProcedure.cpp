// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Procedure/EventHandle_LeaveProcedure.h"

UEventHandle_LeaveProcedure::UEventHandle_LeaveProcedure()
{
	EventType = EEventType::Multicast;
}

void UEventHandle_LeaveProcedure::OnDespawn_Implementation(bool bRecovery)
{
	Procedure = nullptr;
}

void UEventHandle_LeaveProcedure::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Procedure = InParams[0].GetObjectValue<UProcedureBase>();
	}
}
