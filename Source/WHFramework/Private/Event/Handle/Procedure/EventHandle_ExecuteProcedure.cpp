// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Procedure/EventHandle_ExecuteProcedure.h"

#include "Procedure/Base/ProcedureBase.h"

UEventHandle_ExecuteProcedure::UEventHandle_ExecuteProcedure()
{
	EventType = EEventType::Multicast;
}

void UEventHandle_ExecuteProcedure::OnDespawn_Implementation()
{
	Procedure = nullptr;
}

void UEventHandle_ExecuteProcedure::Fill_Implementation(const TArray<FParameter>& InParameters)
{
	if(InParameters.IsValidIndex(0))
	{
		Procedure = InParameters[0].GetObjectValue<UProcedureBase>();
	}
}
