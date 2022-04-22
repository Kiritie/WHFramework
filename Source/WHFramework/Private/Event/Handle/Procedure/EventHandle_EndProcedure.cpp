// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Procedure/EventHandle_EndProcedure.h"

UEventHandle_EndProcedure::UEventHandle_EndProcedure()
{
	EventType = EEventType::Multicast;
}

void UEventHandle_EndProcedure::OnDespawn_Implementation()
{
}

void UEventHandle_EndProcedure::Fill_Implementation(const TArray<FParameter>& InParameters)
{
}
