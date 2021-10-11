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
	FuncName = NAME_None;
}

bool UEventHandle_ExecuteProcedure::Filter_Implementation(UObject* InOwner, const FName InFuncName)
{
	if(InOwner->IsA(AProcedureBase::StaticClass()))
	{
		return InOwner == Procedure && InFuncName == FuncName;
	}
	return true;
}

void UEventHandle_ExecuteProcedure::Fill_Implementation(const TArray<FParameter>& InParameters)
{
	if(InParameters.IsValidIndex(0))
	{
		Procedure = InParameters[0].GetObjectValue<AProcedureBase>();
	}
	if(InParameters.IsValidIndex(1))
	{
		FuncName = *InParameters[1].GetStringValue();
	}
}
