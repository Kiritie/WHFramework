// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Step/EventHandle_ExecuteStep.h"

#include "Step/Base/StepBase.h"

UEventHandle_ExecuteStep::UEventHandle_ExecuteStep()
{

}

void UEventHandle_ExecuteStep::OnDespawn_Implementation(bool bRecovery)
{
	Step = nullptr;
}

void UEventHandle_ExecuteStep::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Step = InParams[0].GetObjectValue<UStepBase>();
	}
}
