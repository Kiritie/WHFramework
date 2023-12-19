// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Step/EventHandle_StepExecuted.h"

#include "Step/Base/StepBase.h"

UEventHandle_StepExecuted::UEventHandle_StepExecuted()
{

}

void UEventHandle_StepExecuted::OnDespawn_Implementation(bool bRecovery)
{
	Step = nullptr;
}

void UEventHandle_StepExecuted::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Step = InParams[0].GetObjectValue<UStepBase>();
	}
}
