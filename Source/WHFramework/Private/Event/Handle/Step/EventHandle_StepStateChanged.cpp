// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Step/EventHandle_StepStateChanged.h"

UEventHandle_StepStateChanged::UEventHandle_StepStateChanged()
{
	Step = nullptr;
}

void UEventHandle_StepStateChanged::OnDespawn_Implementation(bool bRecovery)
{
	Step = nullptr;
}

void UEventHandle_StepStateChanged::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Step = InParams[0];
	}
}
