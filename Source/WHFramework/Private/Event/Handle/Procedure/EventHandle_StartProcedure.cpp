// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Procedure/EventHandle_StartProcedure.h"

UEventHandle_StartProcedure::UEventHandle_StartProcedure()
{
	EventType = EEventType::Multicast;
	RootProcedureIndex = -1;
}

void UEventHandle_StartProcedure::OnDespawn_Implementation()
{
	RootProcedureIndex = -1;
}

void UEventHandle_StartProcedure::Fill_Implementation(const TArray<FParameter>& InParameters)
{
	if(InParameters.IsValidIndex(0))
	{
		RootProcedureIndex = InParameters[0].GetIntegerValue();
	}
}
