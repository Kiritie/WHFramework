// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Procedure/EventHandle_ProcedureEntered.h"

#include "Procedure/Base/ProcedureBase.h"

UEventHandle_ProcedureEntered::UEventHandle_ProcedureEntered()
{
	Procedure = nullptr;
}

void UEventHandle_ProcedureEntered::OnDespawn_Implementation(bool bRecovery)
{
	Procedure = nullptr;
}

void UEventHandle_ProcedureEntered::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Procedure = InParams[0];
	}
}
