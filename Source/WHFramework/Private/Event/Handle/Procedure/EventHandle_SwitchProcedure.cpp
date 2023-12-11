// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Procedure/EventHandle_SwitchProcedure.h"

#include "Procedure/Base/ProcedureBase.h"

UEventHandle_SwitchProcedure::UEventHandle_SwitchProcedure()
{
	ProcedureClass = nullptr;
}

void UEventHandle_SwitchProcedure::OnDespawn_Implementation(bool bRecovery)
{
	ProcedureClass = nullptr;
}

void UEventHandle_SwitchProcedure::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		ProcedureClass = InParams[0].GetClassValue();
	}
}

TArray<FParameter> UEventHandle_SwitchProcedure::Pack_Implementation() const
{
	return { ProcedureClass.Get() };
}
