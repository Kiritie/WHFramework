// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Step/EventHandle_StepEntered.h"

UEventHandle_StepEntered::UEventHandle_StepEntered()
{
	Step = nullptr;
}

void UEventHandle_StepEntered::OnDespawn_Implementation(bool bRecovery)
{
	Step = nullptr;
}

void UEventHandle_StepEntered::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Step = InParams[0].GetObjectValue<UStepBase>();
	}
}
