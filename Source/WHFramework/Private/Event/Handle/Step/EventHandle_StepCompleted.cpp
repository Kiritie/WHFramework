// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Step/EventHandle_StepCompleted.h"

#include "Step/Base/StepBase.h"

UEventHandle_StepCompleted::UEventHandle_StepCompleted()
{
	Step = nullptr;
}

void UEventHandle_StepCompleted::OnDespawn_Implementation(bool bRecovery)
{
	Step = nullptr;
}

void UEventHandle_StepCompleted::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Step = InParams[0].GetObjectValue<UStepBase>();
	}
}
