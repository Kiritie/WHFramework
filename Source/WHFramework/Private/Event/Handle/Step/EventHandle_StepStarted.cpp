// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Step/EventHandle_StepStarted.h"

UEventHandle_StepStarted::UEventHandle_StepStarted()
{
	RootStepIndex = -1;
}

void UEventHandle_StepStarted::OnDespawn_Implementation(bool bRecovery)
{
	RootStepIndex = -1;
}

void UEventHandle_StepStarted::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		RootStepIndex = InParams[0].GetIntegerValue();
	}
}
