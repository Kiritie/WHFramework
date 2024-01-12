// Fill out your copyright notice in the Description page of Project Settings.

#include "Event/Handle/Parameter/EventHandle_GlobalParameterChanged.h"

UEventHandle_GlobalParameterChanged::UEventHandle_GlobalParameterChanged()
{
	ParameterName = NAME_None;
	Parameter = FParameter();
}

void UEventHandle_GlobalParameterChanged::OnDespawn_Implementation(bool bRecovery)
{
	ParameterName = NAME_None;
	Parameter = FParameter();
}

void UEventHandle_GlobalParameterChanged::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		ParameterName = InParams[0].GetNameValue();
	}
	if(InParams.IsValidIndex(1))
	{
		Parameter = InParams[1].GetPointerValueRef<FParameter>();
	}
}
