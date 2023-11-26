// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Step/EventHandle_LeaveStep.h"

UEventHandle_LeaveStep::UEventHandle_LeaveStep()
{

}

void UEventHandle_LeaveStep::OnDespawn_Implementation(bool bRecovery)
{
	Step = nullptr;
}

void UEventHandle_LeaveStep::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Step = InParams[0].GetObjectValue<UStepBase>();
	}
}
