// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Procedure/EventHandle_ProcedureStateChanged.h"

UEventHandle_ProcedureStateChanged::UEventHandle_ProcedureStateChanged()
{
	Procedure = nullptr;
}

void UEventHandle_ProcedureStateChanged::OnDespawn_Implementation(bool bRecovery)
{
	Procedure = nullptr;
}

void UEventHandle_ProcedureStateChanged::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Procedure = InParams[0];
	}
}
