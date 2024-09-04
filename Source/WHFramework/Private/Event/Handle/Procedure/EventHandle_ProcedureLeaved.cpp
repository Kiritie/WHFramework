// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Procedure/EventHandle_ProcedureLeaved.h"

UEventHandle_ProcedureLeaved::UEventHandle_ProcedureLeaved()
{
	Procedure = nullptr;
}

void UEventHandle_ProcedureLeaved::OnDespawn_Implementation(bool bRecovery)
{
	Procedure = nullptr;
}

void UEventHandle_ProcedureLeaved::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Procedure = InParams[0];
	}
}
