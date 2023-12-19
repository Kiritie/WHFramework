// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Step/EventHandle_StepLeaved.h"

UEventHandle_StepLeaved::UEventHandle_StepLeaved()
{

}

void UEventHandle_StepLeaved::OnDespawn_Implementation(bool bRecovery)
{
	Step = nullptr;
}

void UEventHandle_StepLeaved::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Step = InParams[0].GetObjectValue<UStepBase>();
	}
}
